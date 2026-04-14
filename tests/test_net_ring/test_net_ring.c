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

/*
 * test_net_ring.c: Network data integrity test for ioeventfd ring I/O.
 *
 * This test acts as an ICMP echo server (like test_net) but additionally
 * verifies the integrity of every received packet payload. The host side
 * sends pings with a known repeating pattern (0xdeadbeef); the guest
 * checks that every ICMP echo request carries that exact pattern.
 *
 * This exercises the ring-based I/O path (when ioeventfd is available)
 * and catches data corruption that could result from ring index errors,
 * missing barriers, or buffer management bugs.
 */

#include "solo5.h"
#include "../../bindings/lib.c"

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

static void put_uint(unsigned long v)
{
    char buf[21];
    int i = sizeof(buf) - 1;

    buf[i] = '\0';
    if (v == 0) {
        buf[--i] = '0';
    } else {
        while (v > 0) {
            buf[--i] = '0' + (v % 10);
            v /= 10;
        }
    }
    puts(&buf[i]);
}

#define ETHERTYPE_IP  0x0800
#define ETHERTYPE_ARP 0x0806
#define HLEN_ETHER    6
#define PLEN_IPV4     4

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

static uint16_t checksum(uint16_t *addr, size_t count)
{
    register long sum = 0;

    while (count > 1) {
        sum += *(unsigned short *)addr++;
        count -= 2;
    }
    if (count > 0)
        sum += *(unsigned char *)addr;
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return ~sum;
}

static uint16_t htons(uint16_t x)
{
    return (x << 8) + (x >> 8);
}

static uint16_t ntohs(uint16_t x)
{
    return (x << 8) + (x >> 8);
}

/*
 * The expected repeating 4-byte pattern.
 * The host sends pings with: ping -p deadbeef
 */
static const uint8_t expected_pattern[4] = {0xde, 0xad, 0xbe, 0xef};

static solo5_handle_t net_handle;
static struct solo5_net_info net_info;
static uint8_t ipaddr[4] = {0x0a, 0x00, 0x00, 0x02}; /* 10.0.0.2 */
static uint8_t ipaddr_brd[4] = {0x0a, 0x00, 0x00, 0xff}; /* 10.0.0.255 */
static uint8_t ipaddr_brdall[4] = {0xff, 0xff, 0xff, 0xff};
static uint8_t macaddr_brd[HLEN_ETHER] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

static unsigned long n_verified = 0;
static unsigned long n_corrupted = 0;

/*
 * Verify that the ICMP payload contains the expected repeating pattern.
 * Returns true if the payload is correct or empty, false if corrupted.
 */
static bool verify_payload(uint8_t *payload, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (payload[i] != expected_pattern[i % 4])
            return false;
    }
    return true;
}

static bool handle_arp(uint8_t *buf)
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
    if (memcmp(p->arp.tpa, ipaddr, PLEN_IPV4))
        return false;

    memcpy(p->ether.target, p->ether.source, HLEN_ETHER);
    memcpy(p->ether.source, net_info.mac_address, HLEN_ETHER);
    memcpy(p->arp.tha, p->arp.sha, HLEN_ETHER);
    memcpy(p->arp.sha, net_info.mac_address, HLEN_ETHER);
    p->arp.op = htons(2);
    memcpy(p->arp.tpa, p->arp.spa, PLEN_IPV4);
    memcpy(p->arp.spa, ipaddr, PLEN_IPV4);
    return true;
}

static bool handle_ip(uint8_t *buf)
{
    struct pingpkt *p = (struct pingpkt *)buf;

    if (p->ip.version_ihl != 0x45)
        return false;
    if (p->ip.proto != 0x01)
        return false;
    if (memcmp(p->ip.dst_ip, ipaddr, PLEN_IPV4) &&
        memcmp(p->ip.dst_ip, ipaddr_brd, PLEN_IPV4) &&
        memcmp(p->ip.dst_ip, ipaddr_brdall, PLEN_IPV4))
        return false;
    if (p->ping.type != 0x08 || p->ping.code != 0x00)
        return false;

    /*
     * Verify payload integrity: the ICMP data starts after the ping header.
     * ping(8) prepends an 8-byte timestamp before the pattern, so we skip
     * that.
     */
    size_t ip_total = ntohs(p->ip.length);
    size_t icmp_len = ip_total - sizeof(struct ip);
    size_t ping_hdr = sizeof(struct ping);
    /*
     * ping(8) prepends a struct timeval to the ICMP data.
     * On 64-bit systems, struct timeval is 16 bytes (two longs).
     */
    size_t timestamp_len = 16;

    if (icmp_len > ping_hdr + timestamp_len) {
        size_t payload_off = ping_hdr + timestamp_len;
        size_t payload_len = icmp_len - payload_off;
        uint8_t *payload = (uint8_t *)&p->ping + payload_off;

        if (!verify_payload(payload, payload_len)) {
            n_corrupted++;
            puts("CORRUPT: packet ");
            put_uint(n_verified + n_corrupted);
            puts(" payload mismatch at byte\n");
            return false;
        }
    }

    n_verified++;

    /* Build echo reply */
    memcpy(p->ether.target, p->ether.source, HLEN_ETHER);
    memcpy(p->ether.source, net_info.mac_address, HLEN_ETHER);
    p->ip.id = 0;
    p->ip.flags_offset = 0;
    memcpy(p->ip.dst_ip, p->ip.src_ip, PLEN_IPV4);
    memcpy(p->ip.src_ip, ipaddr, PLEN_IPV4);
    p->ip.checksum = 0;
    p->ip.checksum = checksum((uint16_t *)&p->ip, sizeof(struct ip));
    p->ping.type = 0x00;
    p->ping.checksum = 0;
    p->ping.checksum =
        checksum((uint16_t *)&p->ping, ntohs(p->ip.length) - sizeof(struct ip));

    return true;
}

static void send_garp(void)
{
    struct arppkt p;
    uint8_t zero[HLEN_ETHER] = {0};

    memcpy(p.ether.source, net_info.mac_address, HLEN_ETHER);
    memcpy(p.ether.target, macaddr_brd, HLEN_ETHER);
    p.ether.type = htons(ETHERTYPE_ARP);
    p.arp.htype = htons(1);
    p.arp.ptype = htons(ETHERTYPE_IP);
    p.arp.hlen = HLEN_ETHER;
    p.arp.plen = PLEN_IPV4;
    p.arp.op = htons(1);
    memcpy(p.arp.sha, net_info.mac_address, HLEN_ETHER);
    memcpy(p.arp.tha, zero, HLEN_ETHER);
    memcpy(p.arp.spa, ipaddr, PLEN_IPV4);
    memcpy(p.arp.tpa, ipaddr, PLEN_IPV4);

    solo5_net_write(net_handle, (uint8_t *)&p, sizeof p);
}

static const solo5_time_t NSEC_PER_SEC = 1000000000ULL;

#define TARGET_PINGS 10000

int solo5_app_main(const struct solo5_start_info *si)
{
    (void)si;

    puts("\n**** Solo5 test_net_ring: ioeventfd data integrity ****\n\n");

    if (solo5_net_acquire("service0", &net_handle, &net_info) != SOLO5_R_OK) {
        puts("Could not acquire 'service0' network\n");
        puts("FAILURE\n");
        return SOLO5_EXIT_FAILURE;
    }

    send_garp();

    puts("Serving ping on 10.0.0.2, verifying payload pattern 0xdeadbeef\n");
    puts("Target: ");
    put_uint(TARGET_PINGS);
    puts(" verified packets\n");

    while (n_verified < TARGET_PINGS) {
        solo5_handle_set_t ready_set = 0;
        uint8_t buf[net_info.mtu + SOLO5_NET_HLEN];
        size_t len;

        solo5_yield(solo5_clock_monotonic() + NSEC_PER_SEC, &ready_set);
        if (!(ready_set & (1U << net_handle)))
            continue;

        solo5_result_t result =
            solo5_net_read(net_handle, buf, sizeof buf, &len);
        if (result != SOLO5_R_OK)
            continue;

        struct ether *p = (struct ether *)buf;

        if (memcmp(p->target, net_info.mac_address, HLEN_ETHER) &&
            memcmp(p->target, macaddr_brd, HLEN_ETHER))
            continue;

        bool handled = false;
        switch (htons(p->type)) {
        case ETHERTYPE_ARP:
            handled = handle_arp(buf);
            break;
        case ETHERTYPE_IP:
            handled = handle_ip(buf);
            break;
        default:
            break;
        }

        if (handled) {
            if (solo5_net_write(net_handle, buf, len) != SOLO5_R_OK) {
                puts("Write error\n");
                puts("FAILURE\n");
                return SOLO5_EXIT_FAILURE;
            }
        }

        if (n_corrupted > 0) {
            puts("Data corruption detected after ");
            put_uint(n_verified);
            puts(" good packets, ");
            put_uint(n_corrupted);
            puts(" corrupted\n");
            puts("FAILURE\n");
            return SOLO5_EXIT_FAILURE;
        }
    }

    puts("Verified ");
    put_uint(n_verified);
    puts(" packets, 0 corrupted\n");
    puts("SUCCESS\n");
    return SOLO5_EXIT_SUCCESS;
}
