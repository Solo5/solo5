/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a unikernel base layer.
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

#include "solo5.h"

/* Liberally copied / reinvented libc bits */

static void *memcpy(void *dst, const void *src, size_t size)
{
    size_t i;

    for (i = 0; i < size; i++)
        ((uint8_t *)dst)[i] = ((uint8_t *)src)[i];
    return dst;
}

static int memcmp(const void *s1, const void *s2, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (((uint8_t *)s1)[i] < ((uint8_t *)s2)[i])
            return -1;
        if (((uint8_t *)s1)[i] > ((uint8_t *)s2)[i])
            return 1;
    }
    return 0;
}

static size_t strlen(const char *s)
{
    size_t len = 0;

    while (*s++)
        len += 1;
    return len;
}

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

#define assert(e) do {                              \
        if (!(e)) {                                 \
            puts("assertion failed: ");             \
            puts(#e);                               \
            puts("\n");                             \
            solo5_exit();                           \
        }                                           \
    } while (0)

#define ETHERTYPE_IP  0x0800
#define ETHERTYPE_ARP 0x0806
#define HLEN_ETHER  6
#define PLEN_IPV4  4

struct ether {
    uint8_t target[HLEN_ETHER];
    uint8_t source[HLEN_ETHER];
    uint16_t type;
};

struct arp {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t op;
    uint8_t sha[HLEN_ETHER];
    uint8_t spa[PLEN_IPV4];
    uint8_t tha[HLEN_ETHER];
    uint8_t tpa[PLEN_IPV4];
};

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
    uint8_t data[0];
};

struct arppkt {
    struct ether ether;
    struct arp arp;
};

struct pingpkt {
    struct ether ether;
    struct ip ip;
    struct ping ping;
};

/* Copied from https://tools.ietf.org/html/rfc1071 */
static uint16_t checksum(uint16_t *addr, size_t count)
{
    /* Compute Internet Checksum for "count" bytes
     * beginning at location "addr".*/
    register long sum = 0;

    while (count > 1)  {
        /*  This is the inner loop */
        sum += * (unsigned short *) addr++;
        count -= 2;
    }

    /* Add left-over byte, if any */
    if (count > 0)
        sum += * (unsigned char *) addr;

    /* Fold 32-bit sum to 16 bits */
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return ~sum;
}

static uint16_t htons(uint16_t x)
{
    return (x << 8) + (x >> 8);
}

static uint8_t dehex(char c)
{
    if (c >= '0' && c <= '9')
        return (c - '0');
    else if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');
    else if (c >= 'A' && c <= 'F')
        return 10 + (c - 'A');
    else
        return 0;
}

static uint8_t buf[1526];
uint8_t ipaddr[4] = { 0x0a, 0x00, 0x00, 0x02 }; /* 10.0.0.2 */
uint8_t ipaddr_brdnet[4] = { 0x0a, 0x00, 0x00, 0xff }; /* 10.0.0.255 */
uint8_t ipaddr_brdall[4] = { 0xff, 0xff, 0xff, 0xff }; /* 255.255.255.255 */
uint8_t macaddr[HLEN_ETHER];
uint8_t macaddr_brd[HLEN_ETHER] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static int handle_arp(uint8_t *buf)
{
    struct arppkt *p = (struct arppkt *)buf;

    if (p->arp.htype != htons(1))
        return 1;

    if (p->arp.ptype != htons(ETHERTYPE_IP))
        return 1;

    if (p->arp.hlen != HLEN_ETHER || p->arp.plen != PLEN_IPV4)
        return 1;

    if (p->arp.op != htons(1))
        return 1;

    if (memcmp(p->arp.tpa, ipaddr, PLEN_IPV4))
        return 1;

    /* reorder ether net header addresses */
    memcpy(p->ether.target, p->ether.source, HLEN_ETHER);
    memcpy(p->ether.source, macaddr, HLEN_ETHER);
    memcpy(p->arp.tha, p->arp.sha, HLEN_ETHER);
    memcpy(p->arp.sha, macaddr, HLEN_ETHER);

    /* request -> reply */
    p->arp.op = htons(2);

    /* spa -> tpa */
    memcpy(p->arp.tpa, p->arp.spa, PLEN_IPV4);

    /* our ip -> spa */
    memcpy(p->arp.spa, ipaddr, PLEN_IPV4);

    return 0;
}

static int handle_ip(uint8_t *buf)
{
    struct pingpkt *p = (struct pingpkt *)buf;

    if (p->ip.version_ihl != 0x45)
        return 1; /* we don't support IPv6, yet :-) */

    if (p->ip.type != 0x00)
        return 1;

    if (p->ip.proto != 0x01)
        return 1; /* not ICMP */

    if (memcmp(p->ip.dst_ip, ipaddr, PLEN_IPV4) &&
        memcmp(p->ip.dst_ip, ipaddr_brdnet, PLEN_IPV4) &&
        memcmp(p->ip.dst_ip, ipaddr_brdall, PLEN_IPV4))
        return 1; /* not ip addressed to us */

    if (p->ping.type != 0x08)
        return 1; /* not an echo request */

    if (p->ping.code != 0x00)
        return 1;

    /* reorder ether net header addresses */
    memcpy(p->ether.target, p->ether.source, HLEN_ETHER);
    memcpy(p->ether.source, macaddr, HLEN_ETHER);

    p->ip.id = 0;
    p->ip.flags_offset = 0;

    /* reorder ip net header addresses */
    memcpy(p->ip.dst_ip, p->ip.src_ip, PLEN_IPV4);
    memcpy(p->ip.src_ip, ipaddr, PLEN_IPV4);

    /* recalculate ip checksum for return pkt */
    p->ip.checksum = 0;
    p->ip.checksum = checksum((uint16_t *) &p->ip, sizeof(struct ip));

    p->ping.type = 0x0; /* change into reply */

    /* recalculate ICMP checksum */
    p->ping.checksum = 0;
    p->ping.checksum = checksum((uint16_t *) &p->ping,
            htons(p->ip.length) - sizeof(struct ip));
    return 0;
}

static void send_garp(void)
{
    struct arppkt p;
    uint8_t zero[HLEN_ETHER] = { 0 };

    /*
     * Send a gratuitous ARP packet announcing our MAC address.
     */
    memcpy(p.ether.source, macaddr, HLEN_ETHER);
    memcpy(p.ether.target, macaddr_brd, HLEN_ETHER);
    p.ether.type = htons(ETHERTYPE_ARP);
    p.arp.htype = htons(1);
    p.arp.ptype = htons(ETHERTYPE_IP);
    p.arp.hlen = HLEN_ETHER;
    p.arp.plen = PLEN_IPV4;
    p.arp.op = htons(1);
    memcpy(p.arp.sha, macaddr, HLEN_ETHER);
    memcpy(p.arp.tha, zero, HLEN_ETHER);
    memcpy(p.arp.spa, ipaddr, PLEN_IPV4);
    memcpy(p.arp.tpa, ipaddr, PLEN_IPV4);

    if (solo5_net_write_sync((uint8_t *)&p, sizeof p) == -1)
        puts("Could not send GARP packet\n");
}

static void ping_serve(int verbose, int limit)
{
    unsigned long received = 0;

    /* XXX this interface should really not return a string */
    char *smac = solo5_net_mac_str();
    for (int i = 0; i < HLEN_ETHER; i++) {
        macaddr[i] = dehex(*smac++) << 4;
        macaddr[i] |= dehex(*smac++);
        smac++;
    }

    puts("Serving ping on 10.0.0.2, with MAC: ");
    puts(solo5_net_mac_str());
    puts("\n");

    send_garp();

    for (;;) {
        struct ether *p = (struct ether *)&buf;
        int len = sizeof(buf);

        /* wait for packet */
        /* XXX doing the below produces an assert in ukvm, look into it */
        /*
        while (solo5_net_read_sync(buf, &len) != 0) {
            solo5_poll(solo5_clock_monotonic() + 1000000000ULL);
        }
        */
        while (solo5_poll(solo5_clock_monotonic() + 1000000000ULL) == 0) {
            ;
        }
        assert(solo5_net_read_sync(buf, &len) == 0);

        if (memcmp(p->target, macaddr, HLEN_ETHER) &&
            memcmp(p->target, macaddr_brd, HLEN_ETHER))
            continue; /* not ether addressed to us */

        switch (htons(p->type)) {
            case ETHERTYPE_ARP:
                if (handle_arp(buf) != 0)
                    goto out;
                if (verbose)
                    puts("Received arp request, sending reply\n");
                break;
            case ETHERTYPE_IP:
                if (handle_ip(buf) != 0)
                    goto out;
                if (verbose)
                    puts("Received ping, sending reply\n");
                break;
            default:
                goto out;
        }

        if (solo5_net_write_sync(buf, len) == -1)
            puts("Write error\n");

        received++;
        if (limit && received == 100000) {
            puts("Limit reached, exiting\n");
            break;
        }

        continue;

out:
        puts("Received non-ping or unsupported packet, dropped\n");
    }
}

int solo5_app_main(char *cmdline)
{
    int verbose = 0;
    int limit = 0;

    puts("\n**** Solo5 standalone test_ping_serve ****\n\n");

    if (strlen(cmdline) >= 1) {
        switch (cmdline[0]) {
        case 'v':
            verbose = 1;
            break;
        case 'l':
            limit = 1;
            break;
        default:
            puts("Error in command line.\n");
            puts("Usage: test_ping_serve [ verbose | limit ]\n");
            return 1;
        }
    }

    ping_serve(verbose, limit);

    puts("SUCCESS\n");

    return 0;
}
