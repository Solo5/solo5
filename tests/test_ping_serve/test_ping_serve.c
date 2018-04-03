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
#include "../../kernel/lib.c"

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

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

static void tohexs(char *dst, uint8_t *src, size_t size)
{
    while (size--) {
        uint8_t n = *src >> 4;
        *dst++ = (n < 10) ? (n + '0') : (n - 10 + 'a');
        n = *src & 0xf;
        *dst++ = (n < 10) ? (n + '0') : (n - 10 + 'a');
        src++;
    }
    *dst = '\0';
}

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

    if (solo5_net_write((uint8_t *)&p, sizeof p) != SOLO5_R_OK)
        puts("Could not send GARP packet\n");
}

static const solo5_time_t NSEC_PER_SEC = 1000000000ULL;

static void ping_serve(int verbose, int limit)
{
    unsigned long received = 0;

    struct solo5_net_info ni;
    solo5_net_info(&ni);
    memcpy(macaddr, ni.mac_address, sizeof macaddr);

    char macaddr_s[(sizeof macaddr * 2) + 1];
    tohexs(macaddr_s, macaddr, sizeof macaddr);
    puts("Serving ping on 10.0.0.2, with MAC: ");
    puts(macaddr_s);
    puts("\n");

    send_garp();

    uint8_t buf[ni.mtu + SOLO5_NET_HLEN];

    for (;;) {
        struct ether *p = (struct ether *)&buf;
        size_t len;

        /* wait for packet */
        while (solo5_net_read(buf, sizeof buf, &len) == SOLO5_R_AGAIN) {
            solo5_yield(solo5_clock_monotonic() + NSEC_PER_SEC);
        }

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

        if (solo5_net_write(buf, len) != SOLO5_R_OK)
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

int solo5_app_main(const struct solo5_start_info *si)
{
    int verbose = 0;
    int limit = 0;

    puts("\n**** Solo5 standalone test_ping_serve ****\n\n");

    if (strlen(si->cmdline) >= 1) {
        switch (si->cmdline[0]) {
        case 'v':
            verbose = 1;
            break;
        case 'l':
            limit = 1;
            break;
        default:
            puts("Error in command line.\n");
            puts("Usage: test_ping_serve [ verbose | limit ]\n");
            return SOLO5_EXIT_FAILURE;
        }
    }

    ping_serve(verbose, limit);

    puts("SUCCESS\n");

    return SOLO5_EXIT_SUCCESS;
}
