#include <string.h>
#include "tap_frame.h"

#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_IPV6 0x86dd
#define PROTO_TCP      6
#define PROTO_UDP      17

/* Checksum */

static inline uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((unsigned)p[0] << 8 | p[1]);
}

static uint64_t csum_add(uint64_t sum, const uint8_t *p, size_t len)
{
    for (; len > 1; p += 2, len -= 2)
        sum += rd16(p);
    if (len)
        sum += (uint32_t)p[0] << 8;
    return sum;
}

static uint16_t csum_fold(uint64_t sum)
{
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return (uint16_t)sum;
}

int frame_classify(const uint8_t *f, size_t len, struct frame_info *fi)
{
    const size_t l3 = FRAME_ETH_HLEN;
    size_t ip_hlen, ip_total, l4, l4_hlen, ip_end;
    uint16_t csum_offset;
    uint8_t proto;
    bool ipv6;

    memset(fi, 0, sizeof *fi);
    if (len < FRAME_ETH_HLEN)
        return 0;

    switch (rd16(f + 12)) {
    case ETHERTYPE_IPV4:
        if (len < l3 + 20 || (f[l3] >> 4) != 4)
            return 0;
        ip_hlen = (size_t)(f[l3] & 0x0f) * 4;
        if (ip_hlen < 20 || len < l3 + ip_hlen)
            return 0;
        if (rd16(f + l3 + 6) & 0x3fff) /* MF or fragment offset */
            return 0;
        proto = f[l3 + 9];
        ip_total = rd16(f + l3 + 2);
        ipv6 = false;
        break;
    case ETHERTYPE_IPV6:
        if (len < l3 + 40 || (f[l3] >> 4) != 6)
            return 0;
        ip_hlen = 40;
        proto = f[l3 + 6]; /* Next Header: no extension header allowed */
        ip_total = 40 + (size_t)rd16(f + l3 + 4);
        ipv6 = true;
        break;
    default:
        return 0;
    }

    if (proto == PROTO_TCP)
        csum_offset = 16;
    else if (proto == PROTO_UDP)
        csum_offset = 6;
    else
        return 0;

    if (ip_total < ip_hlen || l3 + ip_total > len)
        return -1;
    ip_end = l3 + ip_total;
    l4 = l3 + ip_hlen;

    if (proto == PROTO_TCP) {
        if (ip_end < l4 + 20)
            return -1;
        l4_hlen = (size_t)(f[l4 + 12] >> 4) * 4;
        if (l4_hlen < 20 || ip_end < l4 + l4_hlen)
            return -1;
    } else /* UDP */ {
        l4_hlen = 8;
        if (ip_end < l4 + l4_hlen)
            return -1;
    }

    fi->eligible = true;
    fi->ipv6 = ipv6;
    fi->l4proto = proto;
    fi->ip_hlen = (uint8_t)ip_hlen;
    fi->l4_hlen = (uint8_t)l4_hlen;
    fi->csum_start = (uint16_t)l4;
    fi->csum_offset = csum_offset;
    fi->hdr_len = (uint16_t)(l4 + l4_hlen);
    fi->ip_end = (uint32_t)ip_end;

    return 0;
}

uint16_t frame_seg(const struct frame_info *fi, uint16_t mtu)
{
    unsigned hdrs = (unsigned)fi->ip_hlen + fi->l4_hlen;
    return mtu > hdrs ? (uint16_t)(mtu - hdrs) : 0;
}

static uint64_t pseudo(const uint8_t *f, const struct frame_info *fi)
{
    const uint8_t *ip = f + FRAME_ETH_HLEN;
    uint32_t l4len = fi->ip_end - fi->csum_start;
    uint64_t sum;

    if (fi->ipv6)
        sum = csum_add(0, ip + 8, 32);
    else
        sum = csum_add(0, ip + 12, 8);

    sum += fi->l4proto;
    sum += (l4len >> 16) + (l4len & 0xffff);

    return sum;
}

uint16_t frame_pseudo_sum(const uint8_t *f, const struct frame_info *fi)
{
    return csum_fold(pseudo(f, fi));
}

bool frame_l4_csum_ok(const uint8_t *f, const struct frame_info *fi)
{
    const uint8_t *l4 = f + fi->csum_start;

    if (fi->l4proto == PROTO_UDP && rd16(l4 + 6) == 0)
        return !fi->ipv6;

    uint64_t sum = pseudo(f, fi);
    sum = csum_add(sum, l4, fi->ip_end - fi->csum_start);
    return csum_fold(sum) == 0xffff;
}

int frame_csum_complete(uint8_t *f, size_t len, size_t start, size_t offset)
{
    if (start >= len || offset > len - start || len - start - offset < 2)
        return -1;

    uint16_t c = (uint16_t)~csum_fold(csum_add(0, f + start, len - start));

    if (c == 0)
        c = 0xffff;

    f[start + offset] = (uint8_t)(c >> 8);
    f[start + offset + 1] = (uint8_t)c;
    return 0;
}
