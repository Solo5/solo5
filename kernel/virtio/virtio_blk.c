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
#define VIRTIO_BLK_T_IN           0
#define VIRTIO_BLK_T_OUT          1
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

struct virtio_blk_req {
    struct virtio_blk_hdr hdr;
    uint8_t data[VIRTIO_BLK_SECTOR_SIZE];

    volatile uint8_t status;
    volatile uint8_t hw_used;
};

#define VIRTQ_BLK_MAX_QUEUE_SIZE 8192
static struct virtio_blk_req *blk_bufs;
static uint8_t *blk_data;
static struct virtq blkq;
#define VIRTQ_BLK  0

static uint16_t virtio_blk_pci_base; /* base in PCI config space */

static int blk_configured;
static uint32_t blk_next_avail;
static uint32_t blk_last_used;

/* WARNING: called in interrupt context */
static void check_blk(void)
{
    volatile struct virtq_used_elem *e;
    struct virtq_desc *desc;
    int dbg = 0;

    for (;;) {
        uint16_t data_idx;
        struct virtio_blk_req *req;

        if ((blkq.used->idx % blkq.num) == blk_last_used)
            break;

        e = &(blkq.used->ring[blk_last_used % blkq.num]);
        desc = &(blkq.desc[e->id]);
        req = (struct virtio_blk_req *)desc->addr;

        if (dbg)
            printf("INTR: BLK: desc=0x%p: type=%d sector=%d\n",
                   desc->addr, req->hdr.type, req->hdr.sector);

        req->hw_used = 1;

        data_idx = desc->next;
        desc = &(blkq.desc[data_idx]);

        if (dbg)
            printf("INTR: BLK: desc=0x%p data=%08x...\n",
                   desc->addr, *(uint32_t *)desc->addr);

        data_idx = desc->next;
        desc = &(blkq.desc[data_idx]);

        if (dbg)
            printf("INTR: BLK: desc=0x%p status=%d\n",
                   desc->addr, *(uint8_t *)desc->addr);

        if (dbg)
            printf("REAP: 0x%p next_avail %d last_used %d\n",
                   desc->addr, blk_next_avail, blk_last_used);

        blk_last_used = (blk_last_used + 1) % blkq.num;
    }
}


static struct virtio_blk_req *virtio_blk_op(uint32_t type,
                                            uint64_t sector,
                                            void *data, int len)
{
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    struct virtio_blk_req *req;
    int dbg = 0;

    if (((blk_next_avail + 3) % blkq.num)
        == ((blk_last_used * 3) % blkq.num)) {
        printf("blk buffer full! next_avail:%d last_used:%d\n",
               blk_next_avail, blk_last_used);
        return NULL;
    }

    /* we perform a copy into the blk buffer to make reclaiming easy */
    assert(len <= VIRTIO_BLK_SECTOR_SIZE);
    req = &blk_bufs[blk_next_avail/3];

    if (req->hw_used) {
        printf("blk buffer full! no unused buffers!\n");
        return NULL;
    }

    req->hdr.type = type;
    req->hdr.ioprio = 0;
    req->hdr.sector = sector;
    req->hw_used = 0;

    if (type == VIRTIO_BLK_T_OUT)
        memcpy(req->data, data, len);

    /* it seems like we need 3 descriptors for a single req */

    if (dbg)
        atomic_printf("REQ BLK: 0x%p type=%d\n", req, req->hdr.type);


    /* the header */
    desc = &(blkq.desc[blk_next_avail]);
    desc->addr = (uint64_t)&req->hdr;
    desc->len = sizeof(struct virtio_blk_hdr);
    desc->next = (blk_next_avail + 1) % blkq.num;
    desc->flags = VIRTQ_DESC_F_NEXT;

    if (dbg)
        atomic_printf("REQ BLK: hdr at 0x%x\n", desc->addr);

    /* the data */
    desc = &(blkq.desc[(blk_next_avail + 1) % blkq.num]);
    desc->addr = (uint64_t)req->data;
    desc->len = len;
    desc->next = (blk_next_avail + 2) % blkq.num;
    desc->flags = VIRTQ_DESC_F_NEXT;
    if (type == VIRTIO_BLK_T_IN)
        desc->flags |= VIRTQ_DESC_F_WRITE;

    if (dbg)
        atomic_printf("REQ BLK: data at 0x%x data=%08x...\n",
                      desc->addr, *(uint32_t *)desc->addr);

    /* the status */
    desc = &(blkq.desc[(blk_next_avail + 2) % blkq.num]);
    desc->addr = (uint64_t)&req->status;
    desc->len = sizeof(uint8_t);
    desc->next = 0;
    desc->flags = VIRTQ_DESC_F_WRITE;

    if (dbg)
        atomic_printf("REQ BLK: status at 0x%x\n", desc->addr);

    if (dbg)
        atomic_printf("BLK: 0x%p next_avail %d last_used %d\n",
                      desc->addr, blk_next_avail, blk_last_used);

    avail = blkq.avail;
    /* Memory barriers should be unnecessary with one processor */
    blkq.avail->ring[avail->idx % blkq.num] = blk_next_avail;

    avail->idx++;
    blk_next_avail = (blk_next_avail + 3) % blkq.num;
    outw(virtio_blk_pci_base + VIRTIO_PCI_QUEUE_NOTIFY, VIRTQ_BLK);

    return req;
}

#if 0
static struct virtio_blk_req *virtio_blk_flush(uint64_t sector)
{
    return virtio_blk_op(VIRTIO_BLK_T_FLUSH, sector, data, len);
}
#endif
static struct virtio_blk_req *virtio_blk_write(uint64_t sector,
                                               void *data, int len)
{
    return virtio_blk_op(VIRTIO_BLK_T_OUT, sector, data, len);
}
static struct virtio_blk_req *virtio_blk_read(uint64_t sector, int len)
{
    return virtio_blk_op(VIRTIO_BLK_T_IN, sector, NULL, len);
}

void virtio_config_block(uint16_t base)
{
    uint8_t ready_for_init = VIRTIO_PCI_STATUS_ACK | VIRTIO_PCI_STATUS_DRIVER;
    uint32_t host_features, guest_features;
    int i;
    int dbg = 1;

    outb(base + VIRTIO_PCI_STATUS, ready_for_init);

    host_features = inl(base + VIRTIO_PCI_HOST_FEATURES);

    if (dbg) {
        uint32_t hf = host_features;

        printf("host features: %x: ", hf);
        for (i = 0; i < 32; i++) {
            if (hf & 0x1)
                printf("%d ", i);
            hf = hf >> 1;
        }
        printf("\n");
    }

    /* don't negotiate anything for now */
    guest_features = 0;
    outl(base + VIRTIO_PCI_GUEST_FEATURES, guest_features);

    virtio_blk_sectors = inq(base + VIRTIO_PCI_CONFIG_OFF);
    printf("Found virtio block device with capacity: %d * %d = %d\n",
           virtio_blk_sectors, VIRTIO_BLK_SECTOR_SIZE,
           virtio_blk_sectors * VIRTIO_BLK_SECTOR_SIZE);

    outw(base + VIRTIO_PCI_QUEUE_SEL, 0);

    blkq.num = inw(base + VIRTIO_PCI_QUEUE_SIZE);

    printf("block queue size is %d\n", blkq.num);
    assert(blkq.num <= VIRTQ_BLK_MAX_QUEUE_SIZE);

    blk_data = memalign(4096, VIRTQ_SIZE(blkq.num));
    assert(blk_data);
    memset(blk_data, 0, VIRTQ_SIZE(blkq.num));
    blk_bufs = calloc(blkq.num, sizeof (struct virtio_blk_req));
    assert(blk_bufs);

    blkq.desc =  (struct virtq_desc *)(blk_data + VIRTQ_OFF_DESC(blkq.num));
    blkq.avail = (struct virtq_avail *)(blk_data + VIRTQ_OFF_AVAIL(blkq.num));
    blkq.used = (struct virtq_used *)(blk_data + VIRTQ_OFF_USED(blkq.num));

    virtio_blk_pci_base = base;
    blk_configured = 1;
    outb(base + VIRTIO_PCI_STATUS, VIRTIO_PCI_STATUS_DRIVER_OK);

    outb(base + VIRTIO_PCI_QUEUE_SEL, 0);
    outl(base + VIRTIO_PCI_QUEUE_PFN, (uint64_t)blk_data
         >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);
}


static uint8_t blk_sector[VIRTIO_BLK_SECTOR_SIZE];

static int virtio_blk_write_sync(uint64_t sector, void *data, int len)
{
    struct virtio_blk_req *req;
    int ret = -1;

    req = virtio_blk_write(sector, data, len);
    if (!req)
        return ret;

    /* XXX need timeout or something, because this can hang... sync
     * should probably go away anyway
     */
    while (!req->hw_used)
        ;

    if (req->status == VIRTIO_BLK_S_OK)
        ret = 0;

    req->hw_used = 0;  /* allow reuse of the blk_buf */

    return ret;
}

static int virtio_blk_read_sync(uint64_t sector, void *data, int *len)
{
    struct virtio_blk_req *req;
    int ret = -1;

    if (*len < VIRTIO_BLK_SECTOR_SIZE)
        return ret;

    req = virtio_blk_read(sector, VIRTIO_BLK_SECTOR_SIZE);
    if (!req)
        return ret;

    /* XXX need timeout or something, because this can hang... sync
     * should probably go away anyway
     */
    while (!req->hw_used)
        ;

    if (req->status == VIRTIO_BLK_S_OK) {
        ret = 0;
        memcpy(data, req->data, *len);
    }

    req->hw_used = 0;  /* allow reuse of the blk_buf */

    return ret;
}

void blk_test(void)
{
    struct virtio_blk_req *req;
    uint8_t filler = 0;
    int i;

    printf("Testing blk read and writes...");
    for (i = 0; i < 10; i++) {
        int i;

        filler++;
        memset(blk_sector, filler, VIRTIO_BLK_SECTOR_SIZE);

        req = virtio_blk_write(0, blk_sector, VIRTIO_BLK_SECTOR_SIZE);
        if (!req)
            return;

        while (!req->hw_used)
            ;

        assert(req->status == VIRTIO_BLK_S_OK);
        req->hw_used = 0;  /* allow reuse of the blk_buf */


        req = virtio_blk_read(0, VIRTIO_BLK_SECTOR_SIZE);
        if (!req)
            return;

        while (!req->hw_used)
            ;

        assert(req->status == VIRTIO_BLK_S_OK);
        for (i = 0; i < VIRTIO_BLK_SECTOR_SIZE; i++)
            assert(req->data[i] == blk_sector[i]);

        req->hw_used = 0;  /* allow reuse of the blk_buf */
        printf(".");
    }
    printf("Done\n");
}


void handle_virtio_blk_interrupt(void)
{
    uint8_t isr_status;

    if (blk_configured) {
        isr_status = inb(virtio_blk_pci_base + VIRTIO_PCI_ISR);
        if (isr_status & VIRTIO_PCI_ISR_HAS_INTR) {
            check_blk();
        }
    }
}

int solo5_blk_write_sync(uint64_t sec, uint8_t *data, int n)
{
    assert(blk_configured);

    return virtio_blk_write_sync(sec, data, n);
}

int solo5_blk_read_sync(uint64_t sec, uint8_t *data, int *n)
{
    assert(blk_configured);

    return virtio_blk_read_sync(sec, data, n);
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
