/* Copyright (c) 2015, IBM
 * Author(s): Dan Williams <djwillia@us.ibm.com>
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

#include "kernel.h"
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

static int blk_configured;


/* Returns the index to the head of the buffers chain. */
static uint16_t virtio_blk_op(uint32_t type,
                              uint64_t sector,
                              void *data, int len)
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
                              void *data, int *len)
{
    uint16_t mask = blkq.num - 1;
    uint16_t head;
    struct io_buffer *data_buf, *status_buf;
    uint8_t status;

    head = virtio_blk_op(type, sector, data, *len);
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
        memcpy(data, data_buf->data, *len);

    return 0;
}

void virtio_config_block(struct pci_config_info *pci)
{
    uint8_t ready_for_init = VIRTIO_PCI_STATUS_ACK | VIRTIO_PCI_STATUS_DRIVER;
    uint32_t host_features, guest_features;

    outb(pci->base + VIRTIO_PCI_STATUS, ready_for_init);

    host_features = inl(pci->base + VIRTIO_PCI_HOST_FEATURES);

    /* don't negotiate anything for now */
    guest_features = 0;
    outl(pci->base + VIRTIO_PCI_GUEST_FEATURES, guest_features);

    virtio_blk_sectors = inq(pci->base + VIRTIO_PCI_CONFIG_OFF);
    printf("Solo5: PCI:%02x:%02x: configured, capacity=%d sectors, "
        "features=0x%x\n",
        pci->bus, pci->dev, virtio_blk_sectors, host_features);

    virtq_init_rings(pci->base, &blkq, 0);

    blkq.bufs = calloc(blkq.num, sizeof (struct io_buffer));
    assert(blkq.bufs);

    virtio_blk_pci_base = pci->base;
    blk_configured = 1;

    /*
     * We don't need to get interrupts every time the device uses our
     * descriptors.
     */

    blkq.avail->flags |= VIRTQ_AVAIL_F_NO_INTERRUPT;

    outb(pci->base + VIRTIO_PCI_STATUS, VIRTIO_PCI_STATUS_DRIVER_OK);
}

int solo5_blk_write_sync(uint64_t sector, uint8_t *data, int n)
{
    assert(blk_configured);

    return virtio_blk_op_sync(VIRTIO_BLK_T_OUT, sector, data, &n);
}

int solo5_blk_read_sync(uint64_t sector, uint8_t *data, int *n)
{
    assert(blk_configured);

    return virtio_blk_op_sync(VIRTIO_BLK_T_IN, sector, data, n);
}

int solo5_blk_sector_size(void)
{
    assert(blk_configured);

    return VIRTIO_BLK_SECTOR_SIZE;
}

uint64_t solo5_blk_sectors(void)
{
    assert(blk_configured);

    return virtio_blk_sectors;
}

int solo5_blk_rw(void)
{
    assert(blk_configured);

    return 1;
}
