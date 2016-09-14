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

/* The feature bitmap for virtio net */
#define VIRTIO_NET_F_CSUM	0     /* Host handles pkts w/ partial csum */
#define VIRTIO_NET_F_GUEST_CSUM	1 /* Guest handles pkts w/ partial csum */
#define VIRTIO_NET_F_MAC (1 << 5) /* Host has given MAC address. */

#define PKT_BUFFER_LEN 1526
struct pkt_buffer {
    uint8_t data[PKT_BUFFER_LEN];
    uint32_t len;
};

/*
 * There is no official max queue size. But we've seen 4096, so let's use the
 * double of that.
 */
#define VIRTQ_NET_MAX_QUEUE_SIZE 8192

static struct pkt_buffer *xmit_bufs;
static struct pkt_buffer *recv_bufs;

static uint8_t *recv_data;
static uint8_t *xmit_data;

static struct virtq recvq;
static struct virtq xmitq;

#define VIRTQ_RECV 0
#define VIRTQ_XMIT 1

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

static uint8_t virtio_net_mac[6];
static char virtio_net_mac_str[18];

static int net_configured;
static uint32_t xmit_next_avail;
static uint32_t recv_next_avail;
static uint32_t xmit_last_used;
static uint32_t recv_last_used;

static int handle_virtio_net_interrupt(void *);

/* WARNING: called in interrupt context */
static void check_xmit(void)
{
    volatile struct virtq_used_elem e;
    struct virtq_desc desc;
    int dbg = 0;

    for (;;) {
        if ((xmitq.used->idx % xmitq.num) == xmit_last_used)
            break;

        e = xmitq.used->ring[xmit_last_used % xmitq.num];
        desc = xmitq.desc[e.id];

        if (dbg)
            printf("REAP: 0x%p next_avail %d last_used %d\n",
                   desc.addr, xmit_next_avail, xmit_last_used);

        xmit_last_used = (xmit_last_used + 1) % xmitq.num;
    }
}

static void recv_load_desc(void)
{
    struct virtq_desc *desc;
    struct virtq_avail *avail;

    desc = &(recvq.desc[recv_next_avail]);
    desc->addr = (uint64_t)recv_bufs[recv_next_avail].data;
    desc->len = PKT_BUFFER_LEN;
    desc->flags = VIRTQ_DESC_F_WRITE;
    avail = recvq.avail;
    /* Memory barriers should be unnecessary with one processor */
    recvq.avail->ring[avail->idx % recvq.num] = recv_next_avail;
    avail->idx++;
    recv_next_avail = (recv_next_avail + 1) % recvq.num;
}

/* WARNING: called in interrupt context */
static void check_recv(void)
{
    volatile struct virtq_used_elem *e;
    struct virtq_desc *desc;
    int i;

    for (;;) {
        if ((recvq.used->idx % recvq.num) == recv_last_used)
            break;

        e = &(recvq.used->ring[recv_last_used % recvq.num]);
        desc = &(recvq.desc[e->id]);

        /* Everything should be in a single descriptor. */
        assert(desc->next == 0);

	if (0)
            printf("RECV: 0x%p next_avail %d last_used %d\n",
                   desc->addr, recv_next_avail, recv_last_used);

        ((struct pkt_buffer *)desc->addr)->len = e->len;

        assert(e->len <= PKT_BUFFER_LEN);
        assert(e->len >= sizeof(struct virtio_net_hdr));

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

        recv_last_used = (recv_last_used + 1) % recvq.num;
    }
}

/* WARNING: called in interrupt context */
int handle_virtio_net_interrupt(void *arg __attribute__((unused)))
{
    uint8_t isr_status;

    if (net_configured) {
        isr_status = inb(virtio_net_pci_base + VIRTIO_PCI_ISR);
        if (isr_status & VIRTIO_PCI_ISR_HAS_INTR) {
            check_xmit();
            check_recv();
            return 1;
        }
    }
    return 0;
}

static void recv_setup(void)
{
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    do {
        desc = &(recvq.desc[recv_next_avail]);
        desc->addr = (uint64_t)recv_bufs[recv_next_avail].data;
        desc->len = PKT_BUFFER_LEN;
        desc->flags = VIRTQ_DESC_F_WRITE;

        avail = recvq.avail;
        /* Memory barriers should be unnecessary with one processor */
        recvq.avail->ring[avail->idx % recvq.num] = recv_next_avail;
        avail->idx++;
        recv_next_avail = (recv_next_avail + 1) % recvq.num;
    } while (recv_next_avail != 0);

    outw(virtio_net_pci_base + VIRTIO_PCI_QUEUE_NOTIFY, VIRTQ_RECV);
}

/* performance note: we perform a copy into the xmit buffer */
int virtio_net_xmit_packet(void *data, int len)
{
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    int dbg = 0;

    if (((xmit_next_avail + 1) % xmitq.num) ==
        (xmit_last_used % xmitq.num)) {
        printf("xmit buffer full! next_avail:%d last_used:%d\n",
               xmit_next_avail, xmit_last_used);
            return -1;
    }

    /* we perform a copy into the xmit buffer to make reclaiming easy */
    assert((len + sizeof(virtio_net_hdr)) <= PKT_BUFFER_LEN);
    memcpy(xmit_bufs[xmit_next_avail].data,
           &virtio_net_hdr, sizeof(virtio_net_hdr));
    memcpy(xmit_bufs[xmit_next_avail].data + sizeof(virtio_net_hdr),
           data, len);

    desc = &(xmitq.desc[xmit_next_avail]);
    desc->addr = (uint64_t) xmit_bufs[xmit_next_avail].data;
    desc->len = sizeof(virtio_net_hdr) + len;
    desc->flags = 0;

    if (dbg)
        atomic_printf("XMIT: 0x%p next_avail %d last_used %d\n",
                      desc->addr, xmit_next_avail, xmit_last_used);

    avail = xmitq.avail;
    /* Memory barriers should be unnecessary with one processor */
    xmitq.avail->ring[avail->idx % xmitq.num] = xmit_next_avail;

    avail->idx++;
    xmit_next_avail = (xmit_next_avail + 1) % xmitq.num;
    outw(virtio_net_pci_base + VIRTIO_PCI_QUEUE_NOTIFY, VIRTQ_XMIT);

    return 0;
}


void virtio_config_network(uint16_t base, unsigned irq)
{
    uint8_t ready_for_init = VIRTIO_PCI_STATUS_ACK | VIRTIO_PCI_STATUS_DRIVER;
    uint32_t host_features, guest_features;
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

    /* get the size of the virt queues */
    outw(base + VIRTIO_PCI_QUEUE_SEL, VIRTQ_RECV);
    recvq.num = inw(base + VIRTIO_PCI_QUEUE_SIZE);
    outw(base + VIRTIO_PCI_QUEUE_SEL, VIRTQ_XMIT);
    xmitq.num = inw(base + VIRTIO_PCI_QUEUE_SIZE);
    assert(recvq.num <= VIRTQ_NET_MAX_QUEUE_SIZE);
    assert(xmitq.num <= VIRTQ_NET_MAX_QUEUE_SIZE);
    printf("net queue size is %d/%d\n", recvq.num, xmitq.num);

    recv_data = memalign(4096, VIRTQ_SIZE(recvq.num));
    assert(recv_data);
    memset(recv_data, 0, VIRTQ_SIZE(recvq.num));
    recv_bufs = calloc(recvq.num, sizeof (struct pkt_buffer));
    assert(recv_bufs);

    recvq.desc = (struct virtq_desc *)(recv_data + VIRTQ_OFF_DESC(recvq.num));
    recvq.avail = (struct virtq_avail *)(recv_data + VIRTQ_OFF_AVAIL(recvq.num));
    recvq.used = (struct virtq_used *)(recv_data + VIRTQ_OFF_USED(recvq.num));

    xmit_data = memalign(4096, VIRTQ_SIZE(xmitq.num));
    assert(xmit_data);
    memset(xmit_data, 0, VIRTQ_SIZE(xmitq.num));
    xmit_bufs = calloc(xmitq.num, sizeof (struct pkt_buffer));
    assert(xmit_bufs);

    xmitq.desc = (struct virtq_desc *)(xmit_data + VIRTQ_OFF_DESC(xmitq.num));
    xmitq.avail = (struct virtq_avail *)(xmit_data + VIRTQ_OFF_AVAIL(xmitq.num));
    xmitq.used = (struct virtq_used *)(xmit_data + VIRTQ_OFF_USED(xmitq.num));

    virtio_net_pci_base = base;
    net_configured = 1;
    intr_register_irq(irq, handle_virtio_net_interrupt, NULL);
    outb(base + VIRTIO_PCI_STATUS, VIRTIO_PCI_STATUS_DRIVER_OK);

    outw(base + VIRTIO_PCI_QUEUE_SEL, VIRTQ_RECV);
    outl(base + VIRTIO_PCI_QUEUE_PFN, (uint64_t) recv_data
         >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);
    outw(base + VIRTIO_PCI_QUEUE_SEL, VIRTQ_XMIT);
    outl(base + VIRTIO_PCI_QUEUE_PFN, (uint64_t) xmit_data
         >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);

    recv_setup();
}

int virtio_net_pkt_poll(void)
{
    if (!net_configured)
        return 0;

    if (recv_next_avail == (recv_last_used % recvq.num))
        return 0;
    else
        return 1;
}

uint8_t *virtio_net_pkt_get(int *size)
{
    struct pkt_buffer *buf;

    if (recv_next_avail == (recv_last_used % recvq.num))
        return NULL;

    buf = &recv_bufs[recv_next_avail];

    /* Remove the virtio_net_hdr */
    *size = buf->len - sizeof(virtio_net_hdr);
    return buf->data + sizeof(virtio_net_hdr);
}

void virtio_net_pkt_put(void)
{
    recv_load_desc();
}

int solo5_net_write_sync(uint8_t *data, int n)
{
    assert(net_configured);

    return virtio_net_xmit_packet(data, n);
}

int solo5_net_read_sync(uint8_t *data, int *n)
{
    uint8_t *pkt;
    int len = *n;

    assert(net_configured);

    pkt = virtio_net_pkt_get(&len);
    if (!pkt)
        return -1;

    assert(len <= *n);
    assert(len <= PKT_BUFFER_LEN);
    *n = len;

    /* also, it's clearly not zero copy */
    memcpy(data, pkt, len);

    virtio_net_pkt_put();

    return 0;
}

char *solo5_net_mac_str(void)
{
    assert(net_configured);

    return virtio_net_mac_str;
}
