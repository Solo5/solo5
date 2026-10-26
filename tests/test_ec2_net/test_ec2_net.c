/*
 * test_ec2_net: network bring-up test for the ec2 bindings' ENA driver.
 *
 * Acquires "service", prints a line for each of the first frames received,
 * gets an address from the VPC's DHCP server (DISCOVER, OFFER, REQUEST,
 * ACK), ARPs for the gateway, then answers ARP and ICMP echo for its
 * address forever, printing "echo replies: N" after each reply, and a
 * status line every 10 s. No exit: the test harness terminates it.
 */

#include "solo5.h"
#include "../../bindings/lib.c"

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

static void putu(uint64_t n)
{
    char buf[21];
    int i = sizeof buf - 1;

    buf[i] = '\0';
    do {
        buf[--i] = '0' + (n % 10);
        n /= 10;
    } while (n);
    puts(&buf[i]);
}

static void putx2(uint8_t b)
{
    const char *hex = "0123456789abcdef";
    char s[3] = {hex[b >> 4], hex[b & 15], 0};
    puts(s);
}

static void putmac(const uint8_t *m)
{
    for (int i = 0; i < 6; i++) {
        if (i)
            puts(":");
        putx2(m[i]);
    }
}

static void putip(const uint8_t *ip)
{
    for (int i = 0; i < 4; i++) {
        if (i)
            puts(".");
        putu(ip[i]);
    }
}

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)(p[0] << 8 | p[1]);
}

static void set16(uint8_t *p, uint16_t v)
{
    p[0] = v >> 8;
    p[1] = v & 0xff;
}

static uint16_t ip_checksum(const uint8_t *p, size_t len)
{
    uint32_t sum = 0;
    for (size_t i = 0; i + 1 < len; i += 2)
        sum += get16(p + i);
    if (len & 1)
        sum += p[len - 1] << 8;
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return (uint16_t)~sum;
}

static solo5_handle_t h;
static uint8_t mac[6];
static uint8_t my_ip[4], gw_ip[4], server_ip[4], offered_ip[4];
static bool have_ip;
static uint8_t frame[2048], out[2048];
static uint64_t rx_frames, tx_frames, echo_replies, arp_replies;
static const uint8_t broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

static void send_frame(size_t len)
{
    if (solo5_net_write(h, out, len) == SOLO5_R_OK)
        tx_frames++;
}

static size_t eth_header(const uint8_t *dst, uint16_t type)
{
    memcpy(out, dst, 6);
    memcpy(out + 6, mac, 6);
    set16(out + 12, type);
    return 14;
}

/* DHCP (RFC 2131): a BOOTREQUEST from 0.0.0.0:68 to 255.255.255.255:67. */
#define DHCP_XID 0x50105ec2u

static void send_dhcp(uint8_t type)
{
    memset(out, 0, sizeof out);
    size_t off = eth_header(broadcast, 0x0800);
    uint8_t *ip = out + off, *udp = ip + 20, *b = udp + 8;

    b[0] = 1; /* BOOTREQUEST */
    b[1] = 1; /* Ethernet */
    b[2] = 6;
    b[4] = (DHCP_XID >> 24) & 0xff, b[5] = (DHCP_XID >> 16) & 0xff;
    b[6] = (DHCP_XID >> 8) & 0xff, b[7] = DHCP_XID & 0xff;
    b[10] = 0x80; /* broadcast flag */
    memcpy(b + 28, mac, 6);
    b[236] = 99, b[237] = 130, b[238] = 83, b[239] = 99; /* magic cookie */
    uint8_t *o = b + 240;
    *o++ = 53, *o++ = 1, *o++ = type;
    if (type == 3) {
        *o++ = 50, *o++ = 4;
        memcpy(o, offered_ip, 4), o += 4;
        *o++ = 54, *o++ = 4;
        memcpy(o, server_ip, 4), o += 4;
    }
    *o++ = 55, *o++ = 2, *o++ = 1, *o++ = 3; /* want: netmask, router */
    *o++ = 255;
    size_t blen = (size_t)(o - b);
    if (blen < 300)
        blen = 300;

    set16(udp, 68);
    set16(udp + 2, 67);
    set16(udp + 4, (uint16_t)(8 + blen));
    /* UDP checksum 0: none, allowed over IPv4. */
    ip[0] = 0x45;
    set16(ip + 2, (uint16_t)(20 + 8 + blen));
    ip[8] = 64;
    ip[9] = 17;
    memset(ip + 16, 0xff, 4);
    set16(ip + 10, ip_checksum(ip, 20));
    send_frame(off + 20 + 8 + blen);
    puts(type == 1 ? "dhcp: DISCOVER sent\n" : "dhcp: REQUEST sent\n");
}

static void send_arp(uint16_t op, const uint8_t *tha, const uint8_t *tpa)
{
    size_t off = eth_header(op == 1 ? broadcast : tha, 0x0806);
    uint8_t *a = out + off;
    set16(a, 1);
    set16(a + 2, 0x0800);
    a[4] = 6, a[5] = 4;
    set16(a + 6, op);
    memcpy(a + 8, mac, 6);
    memcpy(a + 14, my_ip, 4);
    if (op == 1)
        memset(a + 18, 0, 6);
    else
        memcpy(a + 18, tha, 6);
    memcpy(a + 24, tpa, 4);
    send_frame(off + 28);
}

static void handle_dhcp(const uint8_t *b, size_t len)
{
    if (len < 240 || b[0] != 2 || memcmp(b + 28, mac, 6) != 0)
        return;
    uint8_t type = 0, router[4] = {0}, server[4] = {0};
    for (size_t i = 240; i + 1 < len && b[i] != 255;) {
        uint8_t opt = b[i], olen = b[i + 1];
        if (opt == 0) {
            i++;
            continue;
        }
        if (i + 2 + olen > len)
            break;
        if (opt == 53 && olen >= 1)
            type = b[i + 2];
        else if (opt == 3 && olen >= 4)
            memcpy(router, b + i + 2, 4);
        else if (opt == 54 && olen >= 4)
            memcpy(server, b + i + 2, 4);
        i += 2 + olen;
    }
    if (type == 2 && !have_ip) {
        memcpy(offered_ip, b + 16, 4);
        memcpy(server_ip, server, 4);
        puts("dhcp: OFFER ");
        putip(offered_ip);
        puts(" from ");
        putip(server_ip);
        puts("\n");
        send_dhcp(3);
    } else if (type == 5 && !have_ip) {
        memcpy(my_ip, b + 16, 4);
        memcpy(gw_ip, router, 4);
        have_ip = true;
        puts("dhcp: ACK, address ");
        putip(my_ip);
        puts(", gateway ");
        putip(gw_ip);
        puts("\n");
        send_arp(1, NULL, gw_ip);
    }
}

static void handle_frame(const uint8_t *f, size_t len)
{
    rx_frames++;
    if (len < 14)
        return;
    uint16_t type = get16(f + 12);
    if (rx_frames <= 20) {
        puts("rx: ");
        putu(len);
        puts(" bytes, ");
        putmac(f + 6);
        puts(" -> ");
        putmac(f);
        puts(", type 0x");
        putx2(type >> 8);
        putx2(type & 0xff);
        puts("\n");
    }

    if (type == 0x0806 && len >= 14 + 28) {
        const uint8_t *a = f + 14;
        uint16_t op = get16(a + 6);
        if (op == 1 && have_ip && memcmp(a + 24, my_ip, 4) == 0) {
            send_arp(2, a + 8, a + 14);
            arp_replies++;
        } else if (op == 2 && have_ip && memcmp(a + 14, gw_ip, 4) == 0) {
            puts("arp: gateway ");
            putip(gw_ip);
            puts(" is at ");
            putmac(a + 8);
            puts("\n");
        }
        return;
    }
    if (type != 0x0800 || len < 14 + 20)
        return;
    const uint8_t *ip = f + 14;
    size_t ihl = (ip[0] & 15) * 4;
    size_t tot = get16(ip + 2);
    if (ihl < 20 || tot < ihl || 14 + tot > len)
        return;

    if (ip[9] == 17 && tot >= ihl + 8) {
        const uint8_t *udp = ip + ihl;
        if (get16(udp) == 67 && get16(udp + 2) == 68)
            handle_dhcp(udp + 8, tot - ihl - 8);
        return;
    }
    if (ip[9] == 1 && have_ip && memcmp(ip + 16, my_ip, 4) == 0 &&
        tot >= ihl + 8 && ip[ihl] == 8) {
        /* Echo request: reply with the same payload. */
        size_t off = eth_header(f + 6, 0x0800);
        uint8_t *rip = out + off;
        memcpy(rip, ip, tot);
        memcpy(rip + 12, my_ip, 4);
        memcpy(rip + 16, ip + 12, 4);
        rip[8] = 64;
        set16(rip + 10, 0);
        set16(rip + 10, ip_checksum(rip, ihl));
        uint8_t *icmp = rip + ihl;
        icmp[0] = 0;
        set16(icmp + 2, 0);
        set16(icmp + 2, ip_checksum(icmp, tot - ihl));
        send_frame(off + tot);
        echo_replies++;
        puts("icmp: echo reply to ");
        putip(ip + 12);
        puts(", echo replies: ");
        putu(echo_replies);
        puts("\n");
    }
}

int solo5_app_main(const struct solo5_start_info *si __attribute__((unused)))
{
    puts("\n**** Solo5 test_ec2_net ****\n\n");

    struct solo5_net_info ni;
    if (solo5_net_acquire("service", &h, &ni) != SOLO5_R_OK) {
        puts("Could not acquire 'service' network device\n");
        return 99;
    }
    memcpy(mac, ni.mac_address, 6);
    puts("service: MAC ");
    putmac(mac);
    puts(", MTU ");
    putu(ni.mtu);
    puts("\n");

    solo5_time_t start = solo5_clock_monotonic();
    solo5_time_t next_status = start + 10000000000ULL;
    solo5_time_t next_dhcp = start;
    for (;;) {
        solo5_time_t now = solo5_clock_monotonic();
        if (!have_ip && now >= next_dhcp) {
            send_dhcp(1);
            next_dhcp = now + 5000000000ULL;
        }
        if (now >= next_status) {
            puts("status: ");
            putu((now - start) / 1000000000ULL);
            puts(" s, rx ");
            putu(rx_frames);
            puts(", tx ");
            putu(tx_frames);
            puts(", arp replies ");
            putu(arp_replies);
            puts(", echo replies ");
            putu(echo_replies);
            puts("\n");
            next_status += 10000000000ULL;
        }

        size_t len;
        while (solo5_net_read(h, frame, sizeof frame, &len) == SOLO5_R_OK)
            handle_frame(frame, len);

        solo5_time_t deadline = next_status;
        if (!have_ip && next_dhcp < deadline)
            deadline = next_dhcp;
        solo5_yield(deadline, NULL);
    }
    return SOLO5_EXIT_SUCCESS;
}
