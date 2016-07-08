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

/* virtio config space layout */
#define VIRTIO_PCI_HOST_FEATURES	0    /* 32-bit r/o */
#define VIRTIO_PCI_GUEST_FEATURES	4    /* 32-bit r/w */
#define VIRTIO_PCI_QUEUE_PFN		8    /* 32-bit r/w */
#define VIRTIO_PCI_QUEUE_SIZE		12   /* 16-bit r/o */
#define VIRTIO_PCI_QUEUE_SEL		14   /* 16-bit r/w */
#define VIRTIO_PCI_QUEUE_NOTIFY		16   /* 16-bit r/w */

/* How many bits to shift physical queue address written to QUEUE_PFN.
 * 12 is historical, and due to x86 page size.
 */
#define VIRTIO_PCI_QUEUE_ADDR_SHIFT	12


/* The status register lets us tell the device where we are in
 * initialization
 */
#define VIRTIO_PCI_STATUS		    18   /* 8-bit r/w */
#define VIRTIO_PCI_STATUS_ACK	    0x1  /* we recognize device as virtio */
#define VIRTIO_PCI_STATUS_DRIVER    0x2  /* we want to drive it */
#define VIRTIO_PCI_STATUS_DRIVER_OK 0x4  /* initialization is complete */
#define VIRTIO_PCI_STATUS_FAIL      0x80 /* tell device something's wrong */

/* Reading the value will return the current contents of the interrupt
 * status register and will also clear it.  This is effectively a
 * read-and-acknowledge.
 */
#define VIRTIO_PCI_ISR			    19   /* 8-bit r/o */
#define VIRTIO_PCI_ISR_HAS_INTR		0x1  /* interrupt is for this device */
#define VIRTIO_PCI_ISR_CONFIG		0x2  /* config change bit */

/* xxx assuming msi is not configured */
#define VIRTIO_PCI_CONFIG_OFF      20


/* Device-independent feature bits */
#define VIRTIO_F_RING_EVENT_IDX (1 << 28)

/* The feature bitmap for virtio net */
#define VIRTIO_NET_F_CSUM	0     /* Host handles pkts w/ partial csum */
#define VIRTIO_NET_F_GUEST_CSUM	1 /* Guest handles pkts w/ partial csum */
#define VIRTIO_NET_F_MAC (1 << 5) /* Host has given MAC address. */


/* Buffer continues via the next field. */
#define VRING_DESC_F_NEXT     1
/* Buffer is write-only (otherwise read-only). */
#define VRING_DESC_F_WRITE    2
/* Buffer contains a list of buffer descriptors. */
#define VRING_DESC_F_INDIRECT 4

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


#define PKT_BUFFER_LEN 1514
struct pkt_buffer {
    uint8_t data[PKT_BUFFER_LEN];
    uint32_t len;
};
struct pkt_buffer xmit_bufs[128];
struct pkt_buffer recv_bufs[128];

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
struct virtio_blk_req blk_bufs[128];

struct __attribute__((__packed__)) vring_desc {
    uint64_t addr;   /* Address (guest-physical). */
    uint32_t len;    /* Length. */
    uint16_t flags;  /* The flags as indicated above. */
    uint16_t next;   /* Next field if flags & NEXT */
};

struct __attribute__((__packed__)) vring_avail_elem {
    uint16_t val;
};

struct __attribute__((__packed__)) vring_avail {
    uint16_t flags;
    uint16_t idx;
    struct vring_avail_elem ring[0];
};

/* u32 is used here for ids for padding reasons. */
struct __attribute__((__packed__)) vring_used_elem {
    uint32_t id;  /* Index of start of used descriptor chain. */
    uint32_t len; /* Total len of the desc chain which was used (written to) */
};

struct __attribute__((__packed__)) vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[0];
};

/* The vring is one of these irritating layouts that we can't just
 *  struct out because of variable length fields.  Instead, we use
 *  these offsets and _get functions.
 */

#define VRING_OFF_DESC(q)  0
#define VRING_OFF_AVAIL(q) ((q) * sizeof(struct vring_desc))
#define VRING_OFF_AVAIL_RING(q) (VRING_OFF_AVAIL(q)             \
                                 + sizeof(struct vring_avail))
#define VRING_OFF_PADDING(q) (VRING_OFF_AVAIL_RING(q)       \
                              + (sizeof(struct vring_avail_elem) * (q)))
#define VRING_OFF_USED(q) ((VRING_OFF_PADDING(q) + PAGE_SIZE - 1) & PAGE_MASK)
#define VRING_OFF_USED_RING(q) (VRING_OFF_USED(q) + sizeof(struct vring_used))

#define VRING_SIZE(q) (VRING_OFF_USED_RING(q)                       \
                       + (sizeof(struct vring_used_elem) * (q)))

struct vring {
    uint32_t size;
    void *vring;
};

struct vring_desc *vring_desc_get(struct vring *vring, int i)
{
    return (struct vring_desc *)(vring->vring
                                 + VRING_OFF_DESC(vring->size)
                                 + (i * sizeof(struct vring_desc)));
}
struct vring_avail *vring_avail_get(struct vring *vring)
{
    return (struct vring_avail *)(vring->vring
                                  + VRING_OFF_AVAIL(vring->size));
}
struct vring_avail_elem *vring_avail_elem_get(struct vring *vring, int i)
{
    return (struct vring_avail_elem *)(vring->vring
                                       + VRING_OFF_AVAIL_RING(vring->size)
                                       + (i * sizeof(struct vring_avail_elem)));
}
struct vring_used *vring_used_get(struct vring *vring)
{
    return (struct vring_used *)(vring->vring
                                 + VRING_OFF_USED(vring->size));
}
struct vring_used_elem *vring_used_elem_get(struct vring *vring, int i)
{
    return (struct vring_used_elem *)(vring->vring
                                      + VRING_OFF_USED_RING(vring->size)
                                      + (i * sizeof(struct vring_used_elem)));
}

#define VRING_NET_QUEUE_SIZE 256

static uint8_t recv_data[VRING_SIZE(VRING_NET_QUEUE_SIZE)] ALIGN_4K;
static uint8_t xmit_data[VRING_SIZE(VRING_NET_QUEUE_SIZE)] ALIGN_4K;
static struct vring recvq = {
    .size = VRING_NET_QUEUE_SIZE,
    .vring = (void *)recv_data,
};
static struct vring xmitq = {
    .size = VRING_NET_QUEUE_SIZE,
    .vring = (void *)xmit_data,
};

#define VIRTQ_RECV 0
#define VIRTQ_XMIT 1
#define VIRTQ_BLK  0


#define VRING_BLK_QUEUE_SIZE 128
static uint8_t blk_data[VRING_SIZE(VRING_BLK_QUEUE_SIZE)] ALIGN_4K;
static struct vring blkq = {
    .size = VRING_BLK_QUEUE_SIZE,
    .vring = (void *)blk_data,
};

/* is our one and only blk IO request completed */
static int inflight_io_completed;

/* This header comes first in the scatter-gather list.
 * If VIRTIO_F_ANY_LAYOUT is not negotiated, it must
 * be the first element of the scatter-gather list.  If you don't
 * specify GSO or CSUM features, you can simply ignore the header.
 */
struct __attribute__((__packed__)) virtio_net_hdr {
#define VIRTIO_NET_HDR_F_NEEDS_CSUM	1   /* Use csum_start, csum_offset */
#define VIRTIO_NET_HDR_F_DATA_VALID	2	/* Csum is valid */
    uint8_t flags;
#define VIRTIO_NET_HDR_GSO_NONE		0	/* Not a GSO frame */
#define VIRTIO_NET_HDR_GSO_TCPV4	1	/* GSO frame, IPv4 TCP (TSO) */
#define VIRTIO_NET_HDR_GSO_UDP		3	/* GSO frame, IPv4 UDP (UFO) */
#define VIRTIO_NET_HDR_GSO_TCPV6	4	/* GSO frame, IPv6 TCP */
#define VIRTIO_NET_HDR_GSO_ECN		0x80	/* TCP has ECN set */
    uint8_t gso_type;
    uint16_t hdr_len;		/* Ethernet + IP + tcp/udp hdrs */
    uint16_t gso_size;		/* Bytes to append to hdr_len per frame */
    uint16_t csum_start;	/* Position to start checksumming from */
	uint16_t csum_offset;	/* Offset after that to place checksum */
};

static struct virtio_net_hdr virtio_net_hdr = {0, 0, 0, 0, 0, 0};
static uint16_t virtio_net_pci_base; /* base in PCI config space */
static uint16_t virtio_blk_pci_base; /* base in PCI config space */

uint8_t virtio_net_mac[6];
static char virtio_net_mac_str[18];

static uint32_t xmit_next_avail;
static uint32_t recv_next_avail;
static uint32_t xmit_last_used;
static uint32_t recv_last_used;

static uint32_t blk_next_avail;
static uint32_t blk_last_used;

/* WARNING: called in interrupt context */
static void check_blk(void)
{
    volatile struct vring_used_elem *e;
    struct vring_desc *desc;
    int dbg = 0;

    for (;;) {
        uint16_t data_idx;
        struct virtio_blk_req *req;

        if ((vring_used_get(&blkq)->idx % blkq.size) == blk_last_used)
            break;

        e = vring_used_elem_get(&blkq, blk_last_used % blkq.size);
        desc = vring_desc_get(&blkq, e->id); /* the virtio_blk header */
        req = (struct virtio_blk_req *)desc->addr;

        if (dbg)
            printf("INTR: BLK: desc=0x%p: type=%d sector=%d\n",
                   desc->addr, req->hdr.type, req->hdr.sector);

        req->hw_used = 1;

        /* TODO */
        inflight_io_completed = 1;

        data_idx = desc->next;
        desc = vring_desc_get(&blkq, data_idx); /* the data buffer */

        if (dbg)
            printf("INTR: BLK: desc=0x%p data=%08x...\n",
                   desc->addr, *(uint32_t *)desc->addr);

        data_idx = desc->next;
        desc = vring_desc_get(&blkq, data_idx); /* the status */

        if (dbg)
            printf("INTR: BLK: desc=0x%p status=%d\n",
                   desc->addr, *(uint8_t *)desc->addr);

        if (dbg)
            printf("REAP: 0x%p next_avail %d last_used %d\n",
                   desc->addr, blk_next_avail, blk_last_used);

        blk_last_used = (blk_last_used + 1) % blkq.size;
    }
}

/* WARNING: called in interrupt context */
static void check_xmit(void)
{
    volatile struct vring_used_elem *e;
    struct vring_desc *desc;
    int dbg = 0;

    for (;;) {
        uint16_t data_idx;

        if ((vring_used_get(&xmitq)->idx % xmitq.size) == xmit_last_used)
            break;

        e = vring_used_elem_get(&xmitq, xmit_last_used % xmitq.size);
        desc = vring_desc_get(&xmitq, e->id); /* the virtio_net header */
        data_idx = desc->next;
        desc = vring_desc_get(&xmitq, data_idx); /* the data buffer */

        if (dbg)
            printf("REAP: 0x%p next_avail %d last_used %d\n",
                   desc->addr, xmit_next_avail, xmit_last_used);

        xmit_last_used = (xmit_last_used + 1) % xmitq.size;
    }
}

static void recv_load_desc(void)
{
    struct vring_desc *desc;
    struct vring_avail *avail;

    /* it seems like we need one descriptor for the virtio header */
    desc = vring_desc_get(&recvq, recv_next_avail);
    desc->addr = (uint64_t)&virtio_net_hdr;
    desc->len = sizeof(virtio_net_hdr);
    desc->next = recv_next_avail + 1;
    desc->flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;

    /* and a separate one for the actual packet */
    desc = vring_desc_get(&recvq, recv_next_avail + 1);
    desc->addr = (uint64_t)recv_bufs[recv_next_avail/2].data;
    desc->len = PKT_BUFFER_LEN;
    desc->flags = VRING_DESC_F_WRITE;

    avail = vring_avail_get(&recvq);
    /* Memory barriers should be unnecessary with one processor */
    vring_avail_elem_get(&recvq, avail->idx % recvq.size)->val
        = recv_next_avail;
    avail->idx++;
    recv_next_avail = (recv_next_avail + 2) % recvq.size;
}

/* WARNING: called in interrupt context */
static void check_recv(void)
{
    volatile struct vring_used_elem *e;
    struct vring_desc *desc;
    int i;

    for (;;) {
        uint16_t data_idx;

        if ((vring_used_get(&recvq)->idx % recvq.size) == recv_last_used)
            break;

        e = vring_used_elem_get(&recvq, recv_last_used % recvq.size);
        desc = vring_desc_get(&recvq, e->id); /* the virtio_net header */
        data_idx = desc->next;
        desc = vring_desc_get(&recvq, data_idx); /* the data buffer */

        if (0)
            printf("RECV: 0x%p next_avail %d last_used %d\n",
                   desc->addr, recv_next_avail, recv_last_used);

        /* record actual packet len */
        ((struct pkt_buffer *)desc->addr)->len = e->len
            - sizeof(struct virtio_net_hdr);

        if (0) {
            printf("recv pkt:\n");
            for (i = 0; i < 64; i++) {
                printf("%02x ", ((uint8_t *)desc->addr)[i]);
                if ((i % 8) == 7)
                    printf(" ");
                if ((i % 16) == 15)
                    printf("\n");
            }
        }

        recv_last_used = (recv_last_used + 1) % recvq.size;
    }
}

/* WARNING: called in interrupt context */
void handle_virtio_interrupt(void)
{
    uint8_t isr_status_net = inb(virtio_net_pci_base + VIRTIO_PCI_ISR);
    uint8_t isr_status_blk = inb(virtio_blk_pci_base + VIRTIO_PCI_ISR);

    if (isr_status_net & VIRTIO_PCI_ISR_HAS_INTR) {
        check_xmit();
        check_recv();
    } else if (isr_status_blk & VIRTIO_PCI_ISR_HAS_INTR) {
        check_blk();
    }
}


static void recv_setup(void)
{
    do {
        recv_load_desc();
    } while (recv_next_avail != 0);

    outw(virtio_net_pci_base + VIRTIO_PCI_QUEUE_NOTIFY, VIRTQ_RECV);
}

/* performance note: we perform a copy into the xmit buffer */
int virtio_net_xmit_packet(void *data, int len)
{
    struct vring_desc *desc;
    struct vring_avail *avail;
    int dbg = 0;

    if (((xmit_next_avail + 2) % xmitq.size)
        == ((xmit_last_used * 2) % xmitq.size)) {
        printf("xmit buffer full! next_avail:%d last_used:%d\n",
               xmit_next_avail, xmit_last_used);
            return -1;
    }

    /* we perform a copy into the xmit buffer to make reclaiming easy */
    assert(len <= PKT_BUFFER_LEN);
    memcpy(xmit_bufs[xmit_next_avail/2].data, data, len);

    /* it seems like we need one descriptor for the virtio header */
    /* should be able to be pulled out (i.e., not set every time) */
    desc = vring_desc_get(&xmitq, xmit_next_avail);
    desc->addr = (uint64_t)&virtio_net_hdr;
    desc->len = sizeof(virtio_net_hdr);
    desc->next = xmit_next_avail + 1;
    desc->flags = VRING_DESC_F_NEXT;

    /* and a separate one for the actual packet */
    desc = vring_desc_get(&xmitq, xmit_next_avail + 1);
    desc->addr = (uint64_t)xmit_bufs[xmit_next_avail/2].data;
    desc->len = len;
    desc->flags = 0;

    if (dbg)
        atomic_printf("XMIT: 0x%p next_avail %d last_used %d\n",
                      desc->addr, xmit_next_avail, xmit_last_used);

    avail = vring_avail_get(&xmitq);
    /* Memory barriers should be unnecessary with one processor */
    vring_avail_elem_get(&xmitq, avail->idx % xmitq.size)->val
        = xmit_next_avail;

    avail->idx++;
    xmit_next_avail = (xmit_next_avail + 2) % xmitq.size;
    outw(virtio_net_pci_base + VIRTIO_PCI_QUEUE_NOTIFY, VIRTQ_XMIT);

    return 0;
}


static struct virtio_blk_req *virtio_blk_op(uint32_t type,
                                            uint64_t sector,
                                            void *data, int len)
{
    struct vring_desc *desc;
    struct vring_avail *avail;
    struct virtio_blk_req *req;
    int dbg = 0;

    if (((blk_next_avail + 3) % blkq.size)
        == ((blk_last_used * 3) % blkq.size)) {
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

    /* TODO */
    inflight_io_completed = 0;

    if (type == VIRTIO_BLK_T_OUT)
        memcpy(req->data, data, len);

    /* it seems like we need 3 descriptors for a single req */

    if (dbg)
        atomic_printf("REQ BLK: 0x%p type=%d\n", req, req->hdr.type);


    /* the header */
    desc = vring_desc_get(&blkq, blk_next_avail);
    desc->addr = (uint64_t)&req->hdr;
    desc->len = sizeof(struct virtio_blk_hdr);
    desc->next = (blk_next_avail + 1) % blkq.size;
    desc->flags = VRING_DESC_F_NEXT;

    if (dbg)
        atomic_printf("REQ BLK: hdr at 0x%x\n", desc->addr);

    /* the data */
    desc = vring_desc_get(&blkq, (blk_next_avail + 1) % blkq.size);
    desc->addr = (uint64_t)req->data;
    desc->len = len;
    desc->next = (blk_next_avail + 2) % blkq.size;
    desc->flags = VRING_DESC_F_NEXT;
    if (type == VIRTIO_BLK_T_IN)
        desc->flags |= VRING_DESC_F_WRITE;

    if (dbg)
        atomic_printf("REQ BLK: data at 0x%x data=%08x...\n",
                      desc->addr, *(uint32_t *)desc->addr);

    /* the status */
    desc = vring_desc_get(&blkq, (blk_next_avail + 2) % blkq.size);
    desc->addr = (uint64_t)&req->status;
    desc->len = sizeof(uint8_t);
    desc->next = 0;
    desc->flags = VRING_DESC_F_WRITE;

    if (dbg)
        atomic_printf("REQ BLK: status at 0x%x\n", desc->addr);

    if (dbg)
        atomic_printf("BLK: 0x%p next_avail %d last_used %d\n",
                      desc->addr, blk_next_avail, blk_last_used);

    avail = vring_avail_get(&blkq);
    /* Memory barriers should be unnecessary with one processor */
    vring_avail_elem_get(&blkq, avail->idx % blkq.size)->val
        = blk_next_avail;

    avail->idx++;
    blk_next_avail = (blk_next_avail + 3) % blkq.size;
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
    uint32_t queue_size;
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

    queue_size = inw(base + VIRTIO_PCI_QUEUE_SIZE);
    printf("queue size is %d\n", queue_size);
    assert(queue_size == VRING_BLK_QUEUE_SIZE);

    outb(base + VIRTIO_PCI_STATUS, VIRTIO_PCI_STATUS_DRIVER_OK);

    outb(base + VIRTIO_PCI_QUEUE_SEL, 0);
    outw(base + VIRTIO_PCI_QUEUE_PFN, (uint64_t)blkq.vring
         >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);

    virtio_blk_pci_base = base;
}

void virtio_config_network(uint16_t base)
{
    uint8_t ready_for_init = VIRTIO_PCI_STATUS_ACK | VIRTIO_PCI_STATUS_DRIVER;
    uint32_t host_features, guest_features;
    uint16_t queue_size;
    int i;
    int dbg = 0;

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

    assert(host_features & VIRTIO_NET_F_MAC);

    /* only negotiate that the mac was set for now */
    guest_features = VIRTIO_NET_F_MAC;
    outl(base + VIRTIO_PCI_GUEST_FEATURES, guest_features);

    printf("Found virtio network device with MAC: ");
    for (i = 0; i < 6; i++) {
        virtio_net_mac[i] = inb(base + VIRTIO_PCI_CONFIG_OFF + i);
        printf("%02x ", virtio_net_mac[i]);
    }
    printf("\n");
    snprintf(virtio_net_mac_str,
             sizeof(virtio_net_mac_str),
             "%02x:%02x:%02x:%02x:%02x:%02x",
             virtio_net_mac[0],
             virtio_net_mac[1],
             virtio_net_mac[2],
             virtio_net_mac[3],
             virtio_net_mac[4],
             virtio_net_mac[5]);

    /* check that 2 256 entry virtqueues are here (recv and transmit) */
    for (i = 0; i > 2; i++) {
        outw(base + VIRTIO_PCI_QUEUE_SEL, i);
        queue_size = inw(base + VIRTIO_PCI_QUEUE_SIZE);
        assert(queue_size == VRING_NET_QUEUE_SIZE);
    }

    outb(base + VIRTIO_PCI_STATUS, VIRTIO_PCI_STATUS_DRIVER_OK);

    outw(base + VIRTIO_PCI_QUEUE_SEL, VIRTQ_RECV);
    outw(base + VIRTIO_PCI_QUEUE_PFN, (uint64_t)recvq.vring
         >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);
    outw(base + VIRTIO_PCI_QUEUE_SEL, VIRTQ_XMIT);
    outw(base + VIRTIO_PCI_QUEUE_PFN, (uint64_t)xmitq.vring
         >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);

    virtio_net_pci_base = base;

    recv_setup();
}


static uint8_t blk_sector[VIRTIO_BLK_SECTOR_SIZE];

static struct virtio_blk_req *virtio_blk_write_async(uint64_t sector,
                                                     void *data, int len)
{
    return virtio_blk_write(sector, data, len);
}

static int virtio_blk_write_async_complete(solo5_request req, int *len)
{
    struct virtio_blk_req *virtio_req;
    int ret = 0;

    virtio_req = (struct virtio_blk_req *) req._req;

    if (virtio_req->status == VIRTIO_BLK_S_OK)
        ret = 0;

    virtio_req->hw_used = 0;  /* allow reuse of the blk_buf */

    /* FIXME: get len */
    len = len;

    /* TODO */
    inflight_io_completed = 0;

    return ret;
}


int virtio_blk_write_sync(solo5_device *dev, uint64_t sec, uint8_t *data, int n)
{
    struct virtio_blk_req *virtio_req;
    solo5_request solo5_req;
    short revents[SOLO5_NUM_DEVICES];
    short events[SOLO5_NUM_DEVICES];

    virtio_req = virtio_blk_write_async(sec, data, n);
    solo5_req._req = (void *) virtio_req;

    memset(events, 0, SOLO5_NUM_DEVICES * sizeof(events));
    events[dev->poll_event_idx] = SOLO5_POLL_IO_READY;
    solo5_poll(solo5_clock_monotonic() + 1e9, events, revents);

    /* TODO */
    inflight_io_completed = 0;

    return virtio_blk_write_async_complete(solo5_req, &n);
}

static struct virtio_blk_req *virtio_blk_read_async_submit(uint64_t sector,
                                                           int *len)
{
    if (*len < VIRTIO_BLK_SECTOR_SIZE)
        return NULL;

    return virtio_blk_read(sector, VIRTIO_BLK_SECTOR_SIZE);
}

static int virtio_blk_read_async_complete(solo5_request req,
                                          void *data, int *len)
{
    struct virtio_blk_req *virtio_req;
    int ret = 0;

    virtio_req = (struct virtio_blk_req *) req._req;

    if (virtio_req->status == VIRTIO_BLK_S_OK) {
        ret = 0;
        memcpy(data, virtio_req->data, *len);
    }

    virtio_req->hw_used = 0;  /* allow reuse of the blk_buf */

    /* TODO */
    inflight_io_completed = 0;

    return ret;
}




int virtio_blk_read_sync(solo5_device *dev, uint64_t sec,
                         uint8_t *data, int *len)
{
    struct virtio_blk_req *virtio_req;
    solo5_request solo5_req;
    short events[SOLO5_NUM_DEVICES];
    short revents[SOLO5_NUM_DEVICES];

    /* TODO: map the device *dev to the virtio device */

    virtio_req = virtio_blk_read_async_submit(sec, len);
    solo5_req._req = (void *) virtio_req;

    memset(events, 0, SOLO5_NUM_DEVICES * sizeof(events));
    events[dev->poll_event_idx] = SOLO5_POLL_IO_READY;
    solo5_poll(solo5_clock_monotonic() + 1e9, events, revents);

    return virtio_blk_read_async_complete(solo5_req, data, len);
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

        /* TODO */
        inflight_io_completed = 0;

        req = virtio_blk_read(0, VIRTIO_BLK_SECTOR_SIZE);
        if (!req)
            return;

        while (!req->hw_used)
            ;

        assert(req->status == VIRTIO_BLK_S_OK);
        for (i = 0; i < VIRTIO_BLK_SECTOR_SIZE; i++)
            assert(req->data[i] == blk_sector[i]);

        req->hw_used = 0;  /* allow reuse of the blk_buf */

        /* TODO */
        inflight_io_completed = 0;

        printf(".");
    }
    printf("Done\n");
}

int virtio_net_pkt_poll(void)
{
    if (recv_next_avail == ((recv_last_used * 2) % recvq.size))
        return 0;
    else
        return 1;
}

uint8_t *virtio_net_pkt_get(int *size)
{
    struct pkt_buffer *buf;

    if (recv_next_avail == ((recv_last_used * 2) % recvq.size))
        return NULL;

    buf = &recv_bufs[recv_next_avail/2];
    *size = buf->len;

    return buf->data;
}
void virtio_net_pkt_put(void)
{
    recv_load_desc();
}



solo5_request solo5_blk_read_async_submit(_UNUSED solo5_device *dev,
                                          uint64_t sec, int *n)
{
    solo5_request req;

    req._req = (void *) virtio_blk_read_async_submit(sec, n);
    return req;
}

int solo5_blk_read_sync(solo5_device *dev, uint64_t sec, uint8_t *data, int *n)
{
    return virtio_blk_read_sync(dev, sec, data, n);
}

int solo5_blk_read_async_complete(_UNUSED solo5_device *dev, solo5_request req,
                                  uint8_t *data, int *n)
{
    return virtio_blk_read_async_complete(req, data, n);
}

solo5_request solo5_blk_write_async(_UNUSED solo5_device *dev, uint64_t sec,
                                    uint8_t *data, int n)
{
    solo5_request req;

    /* TODO: map the device *dev to the virtio device */

    req._req = (void *) virtio_blk_write_async(sec, data, n);
    return req;
}

int solo5_blk_write_sync(solo5_device *dev, uint64_t sec, uint8_t *data, int n)
{
    return virtio_blk_write_sync(dev, sec, data, n);
}

int solo5_blk_write_async_complete(_UNUSED solo5_device *dev,
                                   solo5_request req, int *n)
{
    /* TODO: map the device *dev to the virtio device */

    return virtio_blk_write_async_complete(req, n);
}


int virtio_blk_completed(void)
{
    return inflight_io_completed;
}

int solo5_blk_sector_size(_UNUSED solo5_device *dev)
{
    return VIRTIO_BLK_SECTOR_SIZE;
}

uint64_t solo5_blk_sectors(_UNUSED solo5_device *dev)
{
    return virtio_blk_sectors;
}

int solo5_blk_rw(_UNUSED solo5_device *dev)
{
    return 1;
}


int solo5_net_write_sync(_UNUSED solo5_device *dev, _UNUSED uint64_t off,
                         uint8_t *data, int n)
{
    return virtio_net_xmit_packet(data, n);
}

int solo5_net_read_sync(_UNUSED solo5_device *dev, _UNUSED uint64_t off,
                        uint8_t *data, int *n)
{
    uint8_t *pkt;
    int len = *n;

    pkt = virtio_net_pkt_get(&len);
    if (!pkt)
        return -1;

    assert(len <= *n);
    *n = len;

    /* also, it's clearly not zero copy */
    memcpy(data, pkt, len);

    virtio_net_pkt_put();

    return 0;
}
char *solo5_net_mac_str(_UNUSED solo5_device *dev)
{
    return virtio_net_mac_str;
}
