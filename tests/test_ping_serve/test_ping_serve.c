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
#define HLEN_ETHER  6
#define PLEN_IPV4  4

struct ether {
    uint8_t target[HLEN_ETHER];
    uint8_t source[HLEN_ETHER];
    uint16_t type;
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

struct pingpkt {
    struct ether ether;
    struct ip ip;
    struct ping ping;
};

static uint16_t checksum(void *ptr, size_t len)
{
    uint32_t sum = 0;
    size_t i;

    assert((len % 2) == 0);

    for (i = 0; i < len; i += 2)
        sum += *(uint16_t *)(ptr + i);

    return ~(((uint16_t)(sum & 0xffff)) + ((uint16_t)(sum >> 16)));
};

static uint16_t htons(uint16_t x)
{
    uint16_t val;
    uint8_t *v = (uint8_t *)&val;

    v[0] = (x >> 8) & 0xff;
    v[1] = x & 0xff;
    return val;
}

static uint8_t dehex(char c)
{
    if (c >= '0' && c <= '9')
        return (c - '0');
    else if (c >= 'a' && c <= 'f')
        return (c - 'a');
    else if (c >= 'A' && c <= 'F')
        return (c - 'A');
    else
        return 0;
}

static uint8_t buf[1526];

static void ping_serve(int quiet)
{
    uint8_t ipaddr[4] = { 0x0a, 0x00, 0x00, 0x02 }; /* 10.0.0.2 */
    uint8_t ipaddr_brdnet[4] = { 0x0a, 0x00, 0x00, 0xff }; /* 10.0.0.255 */
    uint8_t ipaddr_brdall[4] = { 0xff, 0xff, 0xff, 0xff }; /* 255.255.255.255 */
    uint8_t macaddr[HLEN_ETHER];
    uint8_t macaddr_brd[HLEN_ETHER] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

    /* XXX this interface should really not return a string */
    char *smac = solo5_net_mac_str();
    for (int i = 0; i < HLEN_ETHER; i++) {
        macaddr[i] = dehex(*smac++) << 4;
        macaddr[i] |= dehex(*smac++);
        smac++;
    }

    puts("Serving ping on 10.0.0.2 / 10.0.0.255 / 255.255.255.255\n");
    puts("With MAC: ");
    puts(solo5_net_mac_str());
    puts(" (no ARP!)\n");

    for (;;) {
        struct pingpkt *p = (struct pingpkt *)&buf;
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

        if (memcmp(p->ether.target, macaddr, HLEN_ETHER) &&
            memcmp(p->ether.target, macaddr_brd, HLEN_ETHER))
            goto out; /* not ether addressed to us */

        if (p->ether.type != htons(ETHERTYPE_IP))
            goto out; /* not an IP packet */

        if (p->ip.version_ihl != 0x45)
            goto out; /* we don't support IPv6, yet :-) */

        if (p->ip.type != 0x00)
            goto out;

        if (p->ip.proto != 0x01)
            goto out; /* not ICMP */

        if (memcmp(p->ip.dst_ip, ipaddr, PLEN_IPV4) &&
            memcmp(p->ip.dst_ip, ipaddr_brdnet, PLEN_IPV4) &&
            memcmp(p->ip.dst_ip, ipaddr_brdall, PLEN_IPV4))
            goto out; /* not ip addressed to us */

        if (p->ping.type != 0x08)
            goto out; /* not an echo request */

        if (p->ping.code != 0x00)
            goto out;

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
        p->ip.checksum = checksum(&p->ip, sizeof(struct ip));

        p->ping.type = 0x0; /* change into reply */

        /* recalculate ICMP checksum */
        p->ping.checksum = 0;
        p->ping.checksum = checksum(&p->ping,
                htons(p->ip.length) - sizeof(struct ip));

        if (!quiet)
            puts("Received ping, sending reply\n");

        if (solo5_net_write_sync(buf, len) == -1)
            puts("Write error\n");

        continue;

out:
        puts("Received non-ping or unsupported packet, dropped\n");
    }
}

int solo5_app_main(char *cmdline)
{
    puts("Hello, World\n");

    /* anything passed on the command line means "quiet" */
    ping_serve(strlen(cmdline));

    return 0;
}
