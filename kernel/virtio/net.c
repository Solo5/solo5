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

#define ETHERTYPE_ARP 0x0806
#define ETHERTYPE_IP  0x0800
#define HTYPE_ETHER 1
#define PTYPE_IPV4  0x0800
#define HLEN_ETHER  6
#define PLEN_IPV4  4
#define OPER_REQUEST  1

static uint8_t virtio_net_ip[4] = { 0x0a, 0x00, 0x00, 0x02 }; /* 10.0.0.2 */

struct ether {
    uint8_t target[HLEN_ETHER];
    uint8_t source[HLEN_ETHER];
    uint16_t type;
};
struct __attribute__((__packed__)) arp {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    uint8_t sha[HLEN_ETHER];
    uint8_t spa[PLEN_IPV4];
    uint8_t tha[HLEN_ETHER];
    uint8_t tpa[PLEN_IPV4];
};

#define IP_PROTO_ICMP  0x01
#define IP_PROTO_UDP   0x11
struct ip {
    uint8_t version_ihl;
    uint8_t type;
    uint16_t length;
    uint16_t id;
    uint16_t flags_offset;
    uint8_t ttl;
    uint8_t proto;
    uint16_t checksum;
    uint8_t src_ip[PLEN_IPV4];
    uint8_t dst_ip[PLEN_IPV4];
};
struct ping {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seqnum;
    uint8_t data[56];
};
struct udp {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
    uint8_t data[4];
};

struct __attribute__((__packed__)) arppkt {
    struct ether ether;
    struct arp arp;
};
struct pingpkt {
    struct ether ether;
    struct ip ip;
    struct ping ping;
};
struct udppkt {
    struct ether ether;
    struct ip ip;
    struct udp udp;
};

static uint16_t checksum(void *ptr, size_t len) {
    uint32_t sum = 0;
    size_t i;
    
    assert( (len % 2) == 0 );

    for (i = 0; i < len; i+= 2)
        sum += *(uint16_t *)(ptr + i);

    return ~(((uint16_t)(sum & 0xffff)) + ((uint16_t)(sum >> 16)));
};

static uint16_t htons(uint16_t x) {
    uint16_t val;
    uint8_t *v = (uint8_t *)&val;
    v[0] = (x >> 8) & 0xff;
    v[1] = x & 0xff;
    return val;
}

#if 0
static void construct_udp(struct udppkt *p, uint8_t *mac_source) {
    size_t i;
    uint8_t mac_target[] = { 0x00, 0x00, 0x0c, 0x07, 0xac, 0x01};
    uint8_t ip_src[] = { 0x09, 0x02, 0x3c, 0x44}; /* 9.2.60.44 */
    uint8_t ip_dst[] = { 0x4a, 0x7d, 0x89, 0x68 };
    uint8_t beef[] = { 0xde, 0xad, 0xbe, 0xef };
    memcpy(p->ether.target, mac_target, HLEN_ETHER);
    memcpy(p->ether.source, mac_source, HLEN_ETHER);
    p->ether.type = htons(ETHERTYPE_IP);

    p->ip.version_ihl = 0x45;
    p->ip.type = 0x00;
    p->ip.length = htons(sizeof(struct ip) + sizeof(struct udp));
    p->ip.id = htons(0x0f01);
    p->ip.flags_offset = 0x0000;
    p->ip.ttl = 0x3d;
    p->ip.proto = IP_PROTO_UDP;
    p->ip.checksum = 0;
    memcpy(p->ip.src_ip, ip_src, PLEN_IPV4);
    memcpy(p->ip.dst_ip, ip_dst, PLEN_IPV4);
    p->ip.checksum = checksum(&p->ip, sizeof(struct ip));

    p->udp.src_port = 0;
    p->udp.dst_port = htons(5000);
    p->udp.length = htons(sizeof(struct udp));
    p->udp.checksum = 0;
    memcpy(p->udp.data, beef, 4);
    p->udp.checksum = checksum(&p->udp, sizeof(struct udp));

    for (i = 0; i < sizeof(struct udppkt); i++) {
        printk("%b ", ((uint8_t *)p)[i]);
        if (i % 8 == 7 )
            printk("   ");
        if (i % 16 == 15 )
            printk("\n");
    }
    printk("\n");
}
#endif

static void construct_ping(struct pingpkt *p, uint8_t *mac_source) {
    size_t i;
    uint8_t mac_target[] = { 0xc2, 0x12, 0x22, 0xee, 0x70, 0x0a};
    uint8_t ping_data[] = { 0xbb, 0xf5, 0x85, 0x54, 0x00, 0x00, 0x00, 0x00,
                            0x3f, 0x2a, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00 };

    uint8_t ip_src[] = { 0x0a, 0x00, 0x04, 0x0a }; /* 10.0.4.10 */
    uint8_t ip_dst[] = { 0x0a, 0x00, 0x04, 0x0b }; /* 10.0.4.11 */

    memcpy(p->ether.target, mac_target, HLEN_ETHER);
    memcpy(p->ether.source, mac_source, HLEN_ETHER);
    p->ether.type = htons(ETHERTYPE_IP);

    p->ip.version_ihl = 0x45;
    p->ip.type = 0x00;
    p->ip.length = htons(0x0054);
    //p->ip.length = htons(0x001c);
    p->ip.id = htons(0x0f01);
    p->ip.flags_offset = 0x0000;
    p->ip.ttl = 0x3d;
    p->ip.proto = 0x01;
    p->ip.checksum = 0;
    memcpy(p->ip.src_ip, ip_src, PLEN_IPV4);
    memcpy(p->ip.dst_ip, ip_dst, PLEN_IPV4);
    p->ip.checksum = checksum(&p->ip, sizeof(struct ip));

    p->ping.type = 0x08;
    p->ping.code = 0x00;
    p->ping.checksum = 0;
    p->ping.id = htons(0x0019);
    p->ping.seqnum = htons(0x9a9a);
    memcpy(p->ping.data, ping_data, 16);
    for ( i = 16; i <=55; i++ )
        *(p->ping.data + i) = i;
    p->ping.checksum = checksum(&p->ping, sizeof(struct ping));

    if(0) {
        for (i = 0; i < sizeof(struct pingpkt); i++) {
            printf("%b ", ((uint8_t *)p)[i]);
            if (i % 8 == 7 )
                printf("   ");
            if (i % 16 == 15 )
                printf("\n");
        }
        printf("\n");
    }
};

#if 0
static void construct_arp(struct arppkt *p, uint8_t *mac){
    uint8_t spa[] = { 0xc0, 0xa8, 0x7a, 0x02 }; /* 192.168.122.2 */
    uint8_t tpa[] = { 0xc0, 0xa8, 0x7a, 0x05 }; /* 192.168.122.5 */
    //struct arppkt *p = &pkt;
    size_t i;

    memset(p->ether.target, 0xff, HLEN_ETHER);
    memcpy(p->ether.source, mac, HLEN_ETHER);
    p->ether.type = htons(ETHERTYPE_ARP);

    p->arp.htype = htons(HTYPE_ETHER);
    p->arp.ptype = htons(PTYPE_IPV4);
    p->arp.hlen = HLEN_ETHER;
    p->arp.plen = PLEN_IPV4;
    p->arp.oper = htons(OPER_REQUEST);
    memcpy(p->arp.sha, mac, HLEN_ETHER);
    memcpy(p->arp.spa, spa, PLEN_IPV4);
    memset(p->arp.tha, 0, HLEN_ETHER);
    memcpy(p->arp.tpa, tpa, PLEN_IPV4);

    for (i = 0; i < sizeof(struct arppkt); i++) {
        printk("%b ", ((uint8_t *)p)[i]);
        if (i % 8 == 7 )
            printk("   ");
        if (i % 16 == 15 )
            printk("\n");
    }
    printk("\n");
}
#endif

struct pingpkt pings[256]; /* there can actually only be 128
                              descriptors loaded at once due to the
                              virtio_net_hdr taking up so many
                              descriptors */

/* XXX Not a "public" API, fix this once we figure out what to do about a
 * runtime for standalone kernel tests. */
void solo5_ping_serve(void) {
    uint8_t *pkt = NULL;
    int i;

    printf("Serving ping on %d.%d.%d.%d\n", 
           virtio_net_ip[0],
           virtio_net_ip[1],
           virtio_net_ip[2],
           virtio_net_ip[3]);

    for (;;) {
        struct pingpkt *p;
        do {
            int len;
            pkt = virtio_net_pkt_get(&len); /* there's a race here */
            if ( pkt == NULL )
                kernel_wait();
        } while(pkt == NULL);

        p = (struct pingpkt *)pkt;

        if ( memcmp(p->ether.target, virtio_net_mac, HLEN_ETHER) )
            goto out; /* not ether addressed to us */
        
        /* reorder ether net header addresses */
        memcpy(p->ether.target, p->ether.source, HLEN_ETHER);
        memcpy(p->ether.source, virtio_net_mac, HLEN_ETHER);

        if ( p->ether.type != htons(ETHERTYPE_IP) )
            goto out; /* not an IP packet */
        
        if ( p->ip.version_ihl != 0x45 )
            goto out;

        if ( p->ip.type != 0x00 )
            goto out;

        if ( p->ip.length != htons(0x54) )
            goto out;

        p->ip.id = 0;
        p->ip.flags_offset = 0;
        
        if ( p->ip.proto != 0x01 )
            goto out; /* not ICMP */

        p->ip.checksum = 0;

        if ( memcmp(p->ip.dst_ip, virtio_net_ip, PLEN_IPV4) )
            goto out; /* not ip addressed to us */
        
        /* reorder ip net header addresses */
        memcpy(p->ip.dst_ip, p->ip.src_ip, PLEN_IPV4);
        memcpy(p->ip.src_ip, virtio_net_ip, PLEN_IPV4);

        /* recalculate ip checksum for return pkt */
        p->ip.checksum = checksum(&p->ip, sizeof(struct ip));

        if ( p->ping.type != 0x08 )
            goto out; /* not an echo request */

        p->ping.type = 0x0; /* change into reply */
        
        if ( p->ping.code != 0x00 )
            goto out; 

        /* we can leave the rest of the ping packet but need to
           recalculate checksum.  We're assuming 56 bytes of data. */
        p->ping.checksum = checksum(&p->ping, sizeof(struct ping));

        printf("got ping...sending ping reply\n");

        if ( virtio_net_xmit_packet(p, sizeof(struct pingpkt)) )
            PANIC("debug this!");

        virtio_net_pkt_put();
        continue;

        if (0) {
            for (i = 0; i < 64; i++) {
                if ((i % 16) == 0) 
                    printf("%04x:  ", i / 16);
                printf("%02x", pkt[i]);
                if ((i % 2) == 1)
                    printf(" ");
                if ((i % 16) == 15)
                    printf("\n");
            }
        }

    out:
        printf("got non-ping...dropping\n");
        virtio_net_pkt_put();
    }
}

void ping_forever(void) {
    struct pingpkt *ping;
    int i;
    //uint8_t recvbuf[1514];
    while(1) {
        for (i = 0; i < 256; i++) {
            ping = &pings[i];
            memset(ping, 0, sizeof(struct pingpkt));
            construct_ping(ping, virtio_net_mac);
            if ( virtio_net_xmit_packet(ping, sizeof(struct pingpkt)) )
                PANIC("debug this!");
            /* sleep(1); */
        }
    }
    /* memset(recvbuf, 0, 1514); */
    /* recv_setup(recvbuf, 1514); */
    /* memcheck(&virtio_net_hdr_r, 0, sizeof(virtio_net_hdr)); */

    /* int i; */
    /* printf("recv pkt:\n"); */
    /* for (i = 0; i < 64; i++) { */
    /*     printf("%02x ", recvbuf[i]); */
    /*     if ((i % 8) == 7) */
    /*         printf(" "); */
    /*     if ((i % 16) == 15) */
    /*         printf("\n"); */
    /* } */

        
    while(1) {
        /* printf("recv pkt:\n"); */
        /* for (i = 0; i < 64; i++) { */
        /*     printf("%02x ", recvbuf[i]); */
        /*     if ((i % 8) == 7) */
        /*         printf(" "); */
        /*     if ((i % 16) == 15) */
        /*         printf("\n"); */
        /* } */
        virtio_net_xmit_packet(ping, sizeof(struct pingpkt));
        //sleep_approx();
    }
}
