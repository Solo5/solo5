/*
 * Copyright (c) 2015-2019 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a sandboxed execution environment.
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "bindings.h"
#include "virtio_ring.h"
#include "virtio_pci.h"

#define VIRTIO_BLK_ID_BYTES       20
#define VIRTIO_BLK_T_IN           0 /* read */
#define VIRTIO_BLK_T_OUT          1 /* write */
#define VIRTIO_BLK_T_SCSI_CMD     2
#define VIRTIO_BLK_T_SCSI_CMD_OUT 3
#define VIRTIO_BLK_T_FLUSH        4
#define VIRTIO_BLK_T_FLUSH_OUT    5
#define VIRTIO_BLK_T_GET_ID       8
#define VIRTIO_BLK_T_BARRIER      0x80000000

#define VIRTIO_BLK_S_OK     0
#define VIRTIO_BLK_S_IOERR  1
#define VIRTIO_BLK_S_UNSUPP 2

static uint64_t virtio_blk_sectors;

#define VIRTIO_BLK_SECTOR_SIZE    512

struct virtio_blk_hdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
};

static struct virtq blkq;
#define VIRTQ_BLK  0

static uint16_t virtio_blk_pci_base; /* base in PCI config space */

static bool blk_configured;
static bool blk_acquired;
static solo5_handle_t blk_handle;
extern struct mft *virtio_manifest;

/* Returns the index to the head of the buffers chain. */
static uint16_t virtio_blk_op(uint32_t type,
                              uint64_t sector,
                              void *data, size_t len)
{
    uint16_t mask = blkq.num - 1;
    struct virtio_blk_hdr hdr;
    struct io_buffer *head_buf, *data_buf, *status_buf;
    uint16_t head = blkq.next_avail & mask;

    assert(len <= VIRTIO_BLK_SECTOR_SIZE);

    head_buf = &blkq.bufs[head];
    data_buf = &blkq.bufs[(head + 1) & mask];
    status_buf = &blkq.bufs[(head + 2) & mask];

    hdr.type = type;
    hdr.ioprio = 0;
    hdr.sector = sector;

    /* The header buf */
    memcpy(head_buf->data, &hdr, sizeof(struct virtio_blk_hdr));
    head_buf->len = sizeof(struct virtio_blk_hdr);
    head_buf->extra_flags = 0;

    /* The data buf */
    if (type == VIRTIO_BLK_T_OUT) /* write */ {
        memcpy(data_buf->data, data, len);
        data_buf->extra_flags = 0;
    } else
        data_buf->extra_flags = VIRTQ_DESC_F_WRITE;
    data_buf->len = len;

    /* The status buf */
    status_buf->len = sizeof(uint8_t);
    status_buf->extra_flags = VIRTQ_DESC_F_WRITE;

    assert(virtq_add_descriptor_chain(&blkq, head, 3) == 0);

    outw(virtio_blk_pci_base + VIRTIO_PCI_QUEUE_NOTIFY, VIRTQ_BLK);

    return head;
}

/*
 * Returns the status (0 is OK, -1 is not)
 *
 * This function assumes that no other pending IO is submitted at the same
 * time.  That is true as long as we use only synchronous IO calls, and if
 * there is just a sync call at a time (which is true for solo5).
 */
static int virtio_blk_op_sync(uint32_t type,
                              uint64_t sector,
                              void *data, size_t len)
{
    uint16_t mask = blkq.num - 1;
    uint16_t head;
    struct io_buffer *data_buf, *status_buf;
    uint8_t status;

    head = virtio_blk_op(type, sector, data, len);
    data_buf = &blkq.bufs[(head + 1) & mask];
    status_buf = &blkq.bufs[(head + 2) & mask];

    /* Loop until the device used all of our descriptors. */
    while (blkq.used->idx != blkq.avail->idx)
        ;

    /* Consume all the recently used descriptors. */
    for (; blkq.used->idx != blkq.last_used; blkq.last_used++) {
        struct virtq_used_elem *e;

        /* Assert that the used descriptor matches the descriptor of the
         * IO we started at the start of this function. */
        e = &(blkq.used->ring[blkq.last_used & mask]);
        assert(head == e->id);

        blkq.num_avail += 3; /* 3 descriptors per chain */
    }

    status = (*(uint8_t *)status_buf);
    if (status != VIRTIO_BLK_S_OK)
        return -1;

    if (type == VIRTIO_BLK_T_IN) /* read */
        memcpy(data, data_buf->data, len);

    return 0;
}

void virtio_config_block(struct pci_config_info *pci)
{
    uint8_t ready_for_init = VIRTIO_PCI_STATUS_ACK | VIRTIO_PCI_STATUS_DRIVER;
    uint32_t host_features, guest_features;
    size_t pgs;

    outb(pci->base + VIRTIO_PCI_STATUS, ready_for_init);

    host_features = inl(pci->base + VIRTIO_PCI_HOST_FEATURES);

    /* don't negotiate anything for now */
    guest_features = 0;
    outl(pci->base + VIRTIO_PCI_GUEST_FEATURES, guest_features);

    virtio_blk_sectors = inq(pci->base + VIRTIO_PCI_CONFIG_OFF);
    log(INFO, "Solo5: PCI:%02x:%02x: configured, capacity=%llu sectors, "
        "features=0x%x\n",
        pci->bus, pci->dev, (unsigned long long)virtio_blk_sectors,
        host_features);

    virtq_init_rings(pci->base, &blkq, 0);

    pgs = (((blkq.num * sizeof (struct io_buffer)) - 1) >> PAGE_SHIFT) + 1;
    blkq.bufs = mem_ialloc_pages(pgs);
    assert(blkq.bufs);
    memset(blkq.bufs, 0, pgs << PAGE_SHIFT);

    virtio_blk_pci_base = pci->base;
    blk_configured = 1;

    /*
     * We don't need to get interrupts every time the device uses our
     * descriptors.
     */

    blkq.avail->flags |= VIRTQ_AVAIL_F_NO_INTERRUPT;

    outb(pci->base + VIRTIO_PCI_STATUS, VIRTIO_PCI_STATUS_DRIVER_OK);
}

/*
 * FIXME: This is a single-device implementation of the new manifest-based
 * APIs. On virtio, this call has the following semantics:
 *
 * 1. The first call to solo5_block_acquire() asking for a handle to a valid
 *    block device (one specified in the application manifest) will succeed,
 *    and return a handle for the sole virtio block device.
 * 2. All subsequent calls will return an error.
 *
 * Note that the presence of a virtio block device is registered during boot
 * in (blk_configured), and the initial acquisition by solo5_block_acquire() is
 * registered in (blk_acquired).
 */
solo5_result_t solo5_block_acquire(const char *name, solo5_handle_t *h,
        struct solo5_block_info *info)
{
    if (!blk_configured || blk_acquired)
        return SOLO5_R_EUNSPEC;

    unsigned mft_index;
    struct mft_entry *mft_e = mft_get_by_name(virtio_manifest, name,
        MFT_DEV_BLOCK_BASIC, &mft_index);
    if (mft_e == NULL)
        return SOLO5_R_EINVAL;
    blk_handle = mft_index;
    blk_acquired = true;

    info->block_size = VIRTIO_BLK_SECTOR_SIZE;
    info->capacity = virtio_blk_sectors * VIRTIO_BLK_SECTOR_SIZE;
    *h = mft_index;
    log(INFO, "Solo5: Application acquired '%s' as block device\n", name);
    return SOLO5_R_OK;
}

solo5_result_t solo5_block_write(solo5_handle_t h, solo5_off_t offset,
        const uint8_t *buf, size_t size)
{
    if (!blk_acquired || h != blk_handle)
        return SOLO5_R_EINVAL;

    /*
     * XXX: This does not check for writes ending past the end of the device,
     * virtio_blk_op_sync() will return -1 (translated to SOLO5_R_EUNSPEC)
     * below.
     */
    uint64_t sector = offset / VIRTIO_BLK_SECTOR_SIZE;
    if ((offset % VIRTIO_BLK_SECTOR_SIZE != 0) ||
        (sector >= virtio_blk_sectors) ||
        (size != VIRTIO_BLK_SECTOR_SIZE))
        return SOLO5_R_EINVAL;

    /*
     * XXX: removing the const qualifier from buf here is fine with the current
     * implementation which does a memcpy() on VIRTIO_BLK_T_OUT, however the
     * internal interfaces should be refactored to reflect this.
     */
    int rv = virtio_blk_op_sync(VIRTIO_BLK_T_OUT, sector, (uint8_t *)buf, size);
    return (rv == 0) ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
}

solo5_result_t solo5_block_read(solo5_handle_t h, solo5_off_t offset,
        uint8_t *buf, size_t size)
{
    if (!blk_acquired || h != blk_handle)
        return SOLO5_R_EINVAL;

    /*
     * XXX: This does not check for reads ending past the end of the device,
     * virtio_blk_op_sync() will return -1 (translated to SOLO5_R_EUNSPEC)
     * below.
     */
    uint64_t sector = offset / VIRTIO_BLK_SECTOR_SIZE;
    if ((offset % VIRTIO_BLK_SECTOR_SIZE != 0) ||
        (sector >= virtio_blk_sectors) ||
        (size != VIRTIO_BLK_SECTOR_SIZE))
        return SOLO5_R_EINVAL;

    int rv = virtio_blk_op_sync(VIRTIO_BLK_T_IN, sector, buf, size);
    return (rv == 0) ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
}
