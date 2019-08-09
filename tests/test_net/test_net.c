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

#include "solo5.h"
#include "../../bindings/lib.c"

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

static void xputs(int ifindex, const char *s)
{
    char which[] = "[serviceX] ";

    which[8] = '0' + ifindex;
    puts(which);
    puts(s);
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

struct netif {
    uint8_t ipaddr[4];
    uint8_t ipaddr_brdnet[4];
    solo5_handle_t h;
    struct solo5_net_info info;
};

struct netif ni[] = {
    {
        .ipaddr = { 0x0a, 0x00, 0x00, 0x02 }, /* 10.0.0.2 */
        .ipaddr_brdnet = { 0x0a, 0x00, 0x00, 0xff } /* 10.0.0.255 */
    },
#ifdef TWO_INTERFACES
    {
        .ipaddr = { 0x0a, 0x01, 0x00, 0x02 }, /* 10.1.0.2 */
        .ipaddr_brdnet = { 0x0a, 0x01, 0x00, 0xff } /* 10.1.0.255 */
    }
#endif
};

uint8_t ipaddr_brdall[4] = { 0xff, 0xff, 0xff, 0xff }; /* 255.255.255.255 */
uint8_t macaddr_brd[HLEN_ETHER] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static unsigned long n_pings_received = 0;
static bool opt_verbose = false;
static bool opt_limit = false;

static bool handle_arp(int ifindex, uint8_t *buf)
{
    struct arppkt *p = (struct arppkt *)buf;

    if (p->arp.htype != htons(1))
        return false;

    if (p->arp.ptype != htons(ETHERTYPE_IP))
        return false;

    if (p->arp.hlen != HLEN_ETHER || p->arp.plen != PLEN_IPV4)
        return false;

    if (p->arp.op != htons(1))
        return false;

    if (memcmp(p->arp.tpa, ni[ifindex].ipaddr, PLEN_IPV4))
        return false;

    /* reorder ether net header addresses */
    memcpy(p->ether.target, p->ether.source, HLEN_ETHER);
    memcpy(p->ether.source, ni[ifindex].info.mac_address, HLEN_ETHER);
    memcpy(p->arp.tha, p->arp.sha, HLEN_ETHER);
    memcpy(p->arp.sha, ni[ifindex].info.mac_address, HLEN_ETHER);

    /* request -> reply */
    p->arp.op = htons(2);

    /* spa -> tpa */
    memcpy(p->arp.tpa, p->arp.spa, PLEN_IPV4);

    /* our ip -> spa */
    memcpy(p->arp.spa, ni[ifindex].ipaddr, PLEN_IPV4);

    return true;
}

static bool handle_ip(int ifindex, uint8_t *buf)
{
    struct pingpkt *p = (struct pingpkt *)buf;

    if (p->ip.version_ihl != 0x45)
        return false; /* we don't support IPv6, yet :-) */

    if (p->ip.type != 0x00)
        return false;

    if (p->ip.proto != 0x01)
        return false; /* not ICMP */

    if (memcmp(p->ip.dst_ip, ni[ifindex].ipaddr, PLEN_IPV4) &&
        memcmp(p->ip.dst_ip, ni[ifindex].ipaddr_brdnet, PLEN_IPV4) &&
        memcmp(p->ip.dst_ip, ipaddr_brdall, PLEN_IPV4))
        return false; /* not ip addressed to us */

    if (p->ping.type != 0x08)
        return false; /* not an echo request */

    if (p->ping.code != 0x00)
        return false;

    /* reorder ether net header addresses */
    memcpy(p->ether.target, p->ether.source, HLEN_ETHER);
    memcpy(p->ether.source, ni[ifindex].info.mac_address, HLEN_ETHER);

    p->ip.id = 0;
    p->ip.flags_offset = 0;

    /* reorder ip net header addresses */
    memcpy(p->ip.dst_ip, p->ip.src_ip, PLEN_IPV4);
    memcpy(p->ip.src_ip, ni[ifindex].ipaddr, PLEN_IPV4);

    /* recalculate ip checksum for return pkt */
    p->ip.checksum = 0;
    p->ip.checksum = checksum((uint16_t *) &p->ip, sizeof(struct ip));

    p->ping.type = 0x0; /* change into reply */

    /* recalculate ICMP checksum */
    p->ping.checksum = 0;
    p->ping.checksum = checksum((uint16_t *) &p->ping,
            htons(p->ip.length) - sizeof(struct ip));

    n_pings_received++;
    return true;
}

static void send_garp(int ifindex)
{
    struct arppkt p;
    uint8_t zero[HLEN_ETHER] = { 0 };

    /*
     * Send a gratuitous ARP packet announcing our MAC address.
     */
    memcpy(p.ether.source, ni[ifindex].info.mac_address, HLEN_ETHER);
    memcpy(p.ether.target, macaddr_brd, HLEN_ETHER);
    p.ether.type = htons(ETHERTYPE_ARP);
    p.arp.htype = htons(1);
    p.arp.ptype = htons(ETHERTYPE_IP);
    p.arp.hlen = HLEN_ETHER;
    p.arp.plen = PLEN_IPV4;
    p.arp.op = htons(1);
    memcpy(p.arp.sha, ni[ifindex].info.mac_address, HLEN_ETHER);
    memcpy(p.arp.tha, zero, HLEN_ETHER);
    memcpy(p.arp.spa, ni[ifindex].ipaddr, PLEN_IPV4);
    memcpy(p.arp.tpa, ni[ifindex].ipaddr, PLEN_IPV4);

    if (solo5_net_write(ni[ifindex].h, (uint8_t *)&p, sizeof p) != SOLO5_R_OK)
        xputs(ifindex, "Could not send GARP packet\n");
}

static const solo5_time_t NSEC_PER_SEC = 1000000000ULL;

static bool handle_packet(int ifindex)
{
    uint8_t buf[ni[ifindex].info.mtu + SOLO5_NET_HLEN];
    solo5_result_t result;
    size_t len;
    struct ether *p = (struct ether *)&buf;
    bool handled = false;

    result = solo5_net_read(ni[ifindex].h, buf, sizeof buf, &len);
    if (result != SOLO5_R_OK) {
        xputs(ifindex, "Read error\n");
        return false;
    }

    if (memcmp(p->target, ni[ifindex].info.mac_address, HLEN_ETHER) &&
        memcmp(p->target, macaddr_brd, HLEN_ETHER))
        return true; /* not ether addressed to us */

    switch (htons(p->type)) {
        case ETHERTYPE_ARP:
            if (handle_arp(ifindex, buf)) {
                handled = true;
                if (opt_verbose)
                    xputs(ifindex, "Received arp request, sending reply\n");
            }
            break;
        case ETHERTYPE_IP:
            if (handle_ip(ifindex, buf)) {
                if (opt_verbose)
                    xputs(ifindex, "Received ping, sending reply\n");
                handled = true;
            }
            break;
        default:
            break;
    }

    if (handled) {
        if (solo5_net_write(ni[ifindex].h, buf, len) != SOLO5_R_OK) {
            xputs(ifindex, "Write error\n");
            return false;
        }
    }
    else {
        xputs(ifindex, "Unknown or unsupported packet, dropped\n");
    }

    return true;
}

static bool ping_serve(void)
{
    if (solo5_net_acquire("service0", &ni[0].h, &ni[0].info) != SOLO5_R_OK) {
        puts("Could not acquire 'service0' network\n");
        return false;
    }
#ifdef TWO_INTERFACES
    if (solo5_net_acquire("service1", &ni[1].h, &ni[1].info) != SOLO5_R_OK) {
        puts("Could not acquire 'service1' network\n");
        return false;
    }
#endif

    char macaddr_s[(HLEN_ETHER * 2) + 1];
    tohexs(macaddr_s, ni[0].info.mac_address, HLEN_ETHER);
    xputs(0, "Serving ping on 10.0.0.2, with MAC: ");
    puts(macaddr_s);
    puts("\n");

    send_garp(0);

#ifdef TWO_INTERFACES
    tohexs(macaddr_s, ni[1].info.mac_address, HLEN_ETHER);
    xputs(1, "Serving ping on 10.1.0.2, with MAC: ");
    puts(macaddr_s);
    puts("\n");

    send_garp(1);
#endif

    for (;;) {
        solo5_handle_set_t ready_set = 0;

        solo5_yield(solo5_clock_monotonic() + NSEC_PER_SEC, &ready_set);
        if (ready_set & 1U << ni[0].h)
            if (!handle_packet(0))
                return false;
#ifdef TWO_INTERFACES
        if (ready_set & 1U << ni[1].h)
            if (!handle_packet(1))
                return false;
#endif
        if (opt_limit && n_pings_received >= 100000) {
            puts("Limit reached, exiting\n");
            break;
        }
    }

    return true;
}

int solo5_app_main(const struct solo5_start_info *si)
{
    puts("\n**** Solo5 standalone test_net ****\n\n");

    if (strlen(si->cmdline) >= 1) {
        switch (si->cmdline[0]) {
        case 'v':
            opt_verbose = true;
            break;
        case 'l':
            opt_limit = true;
            break;
        default:
            puts("Error in command line.\n");
            puts("Usage: test_net [ verbose | limit ]\n");
            return SOLO5_EXIT_FAILURE;
        }
    }

    if (ping_serve()) {
        puts("SUCCESS\n");
        return SOLO5_EXIT_SUCCESS;
    }
    else {
        puts("FAILURE\n");
        return SOLO5_EXIT_FAILURE;
    }
}
