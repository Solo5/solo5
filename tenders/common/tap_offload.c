/*
 * Copyright (c) 2015-2026 Contributors as noted in the AUTHORS file
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

#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "solo5.h"
#include "tap_attach.h"
#include "tap_offload.h"
#include "tap_frame.h"

#if defined(__linux__)

#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>

static int os_hdr_enabled(int fd)
{
    struct ifreq ifr;

    memset(&ifr, 0, sizeof ifr);
    if (ioctl(fd, TUNGETIFF, &ifr) == -1)
        return -1;
    return (ifr.ifr_flags & IFF_VNET_HDR) != 0;
}

static int os_setup(struct tap *d, const char *ifname, uint32_t offloads)
{
    int mtu = -1, fd, hdrsz = 12;
    unsigned tunf = 0;

    if (ifname[0] == '@') {
        if ((fd = tap_attach(ifname, &mtu)) < 0)
            return -1;
        if (os_hdr_enabled(fd) != 1) {
            warnx("%s: fd opened without IFF_VNET_HDR", ifname);
            errno = ENOTSUP;
            goto fail;
        }
    } else if ((fd = tap_attach_flags(ifname, &mtu, IFF_VNET_HDR)) < 0)
        return -1;

    if (ioctl(fd, TUNSETVNETHDRSZ, &hdrsz) == -1)
        goto fail;

    if (offloads & SOLO5_NET_OFFLOAD_RX_CSUM)
        tunf |= TUN_F_CSUM;
    if (offloads & SOLO5_NET_OFFLOAD_RX_GRO)
        tunf |= TUN_F_CSUM | TUN_F_TSO4 | TUN_F_TSO6;
    if (ioctl(fd, TUNSETOFFLOAD, tunf) == -1)
        goto fail;

    d->fd = fd;
    d->kind = TAP_HDR_VIRTIO;
    d->mtu = mtu;
    d->offloads = SOLO5_NET_OFFLOAD_TX_CSUM | SOLO5_NET_OFFLOAD_TX_TSO4 |
                  SOLO5_NET_OFFLOAD_TX_TSO6 | SOLO5_NET_OFFLOAD_TX_USO |
                  SOLO5_NET_OFFLOAD_RX_CSUM | SOLO5_NET_OFFLOAD_RX_GRO;
    d->max_frame = 14 + 65535;
    return 0;
fail:
    {
        int e = errno;
        close(fd);
        errno = e;
    } /* Keep old errno */
    return -1;
}

#elif defined(__FreeBSD__)

#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <net/if_tap.h>

static int os_hdr_enabled(int fd)
{
#ifdef TAPGVNETHDR
    int len = 0;

    if (ioctl(fd, TAPGVNETHDR, &len) == -1)
        return -1;
    return len != 0;
#else
    (void)fd;
    return 0;
#endif
}

static int os_setup(struct tap *d, const char *ifname, uint32_t offloads)
{
#ifdef TAPSVNETHDR
    int mtu = -1, fd, hdrsz = 12, sock;
    struct ifreq ifr;

    if (ifname[0] == '@') {
        errno = ENOTSUP;
        return -1;
    }
    if ((fd = tap_attach(ifname, &mtu)) < 0)
        return -1;
    if (ioctl(fd, TAPSVNETHDR, &hdrsz) == -1)
        goto fail;
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        goto fail;
    memset(&ifr, 0, sizeof ifr);
    strlcpy(ifr.ifr_name, ifname, sizeof ifr.ifr_name);
    if (ioctl(sock, SIOCGIFCAP, &ifr) == -1)
        goto fail_sock;

    int caps = ifr.ifr_curcap &
               ~(IFCAP_TXCSUM | IFCAP_TXCSUM_IPV6 | IFCAP_TSO4 | IFCAP_TSO6);
    if (offloads & SOLO5_NET_OFFLOAD_RX_CSUM)
        caps |= IFCAP_TXCSUM | IFCAP_TXCSUM_IPV6;
    if (offloads & SOLO5_NET_OFFLOAD_RX_GRO)
        caps |= IFCAP_TSO4 | IFCAP_TSO6;
    ifr.ifr_reqcap = caps;

    if (ioctl(sock, SIOCSIFCAP, &ifr) == -1)
        goto fail_sock;
    if (ioctl(sock, SIOCGIFCAP, &ifr) == -1) /* Read back */
        goto fail_sock;
    close(sock);

    d->fd = fd;
    d->kind = TAP_HDR_VIRTIO;
    d->mtu = mtu;
    d->offloads = SOLO5_NET_OFFLOAD_TX_CSUM;
    if (ifr.ifr_curcap & IFCAP_TXCSUM)
        d->offloads |= SOLO5_NET_OFFLOAD_RX_CSUM;
    if (ifr.ifr_curcap & IFCAP_TSO4)
        d->offloads |= SOLO5_NET_OFFLOAD_RX_GRO;
    d->max_frame = 14 + 65535;
    return 0;
fail_sock:
    close(sock);
fail:
    {
        int e = errno;
        close(fd);
        errno = e;
    }
    return -1;
#else
    errno = ENOTSUP;
    return -1;
#endif
}

#elif defined(__OpenBSD__)

#include <sys/socket.h>
#include <net/if.h>
#include <net/if_tun.h>

static int os_hdr_enabled(int fd)
{
#ifdef TUNGCAP
    struct tun_capabilities cap;

    memset(&cap, 0, sizeof cap);
    if (ioctl(fd, TUNGCAP, &cap) == -1)
        return errno == ENODEV ? 0 : -1;
    return cap.tun_if_capabilities != 0;
#else
    (void)fd;
    return 0;
#endif
}

static int os_setup(struct tap *d, const char *ifname, uint32_t offloads)
{
#ifdef TUNSCAP
    int mtu = -1, fd;
    struct tun_capabilities cap = {0};

    if ((fd = tap_attach(ifname, &mtu)) < 0)
        return -1;
    if (offloads & SOLO5_NET_OFFLOAD_RX_CSUM)
        cap.tun_if_capabilities |= IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4 |
                                   IFCAP_CSUM_TCPv6 | IFCAP_CSUM_UDPv6;
    if (offloads & SOLO5_NET_OFFLOAD_RX_GRO)
        cap.tun_if_capabilities |= IFCAP_TSOv4 | IFCAP_TSOv6;

    if (ioctl(fd, TUNSCAP, &cap) == -1) {
        if (!(cap.tun_if_capabilities & (IFCAP_TSOv4 | IFCAP_TSOv6)))
            goto fail;
        /* Retry without TSO */
        cap.tun_if_capabilities &= ~(IFCAP_TSOv4 | IFCAP_TSOv6);
        if (ioctl(fd, TUNSCAP, &cap) == -1)
            goto fail;
    }
    if (ioctl(fd, TUNGCAP, &cap) == -1) /* Read back */
        goto fail;

    d->fd = fd;
    d->kind = TAP_HDR_OPENBSD;
    d->mtu = mtu;
    d->offloads = SOLO5_NET_OFFLOAD_TX_CSUM | SOLO5_NET_OFFLOAD_TX_TSO4 |
                  SOLO5_NET_OFFLOAD_TX_TSO6;

    if (cap.tun_if_capabilities & IFCAP_CSUM_TCPv4)
        d->offloads |= SOLO5_NET_OFFLOAD_RX_CSUM;
    if (cap.tun_if_capabilities & IFCAP_TSOv4)
        d->offloads |= SOLO5_NET_OFFLOAD_RX_GRO;

    d->max_frame = 16384; /* TODO(dinosaure): to confirm! */
    return 0;
fail:
    {
        int e = errno;
        close(fd);
        errno = e;
    }
    return -1;
#else
    errno = ENOTSUP;
    return -1;
#endif
}

#elif defined(__DragonFly__)

static int os_hdr_enabled(int fd)
{
    (void)fd;
    return 0;
}

static int os_setup(struct tap *d, const char *ifname, uint32_t offloads)
{
    (void)d;
    (void)ifname;
    (void)offloads;
    errno = ENOTSUP;
    return -1;
}

#else /* !__linux__ && !__FreeBSD__ && !__OpenBSD__ && !__DragonFly__ */

#error Unsupported target

#endif

#define OFFLOAD_TX_ANY                                                         \
    (SOLO5_NET_OFFLOAD_TX_TSO4 | SOLO5_NET_OFFLOAD_TX_TSO6 |                   \
     SOLO5_NET_OFFLOAD_TX_USO)

static uint32_t offload_normalize(uint32_t v)
{
    if (!(v & SOLO5_NET_OFFLOAD_TX_CSUM))
        v &= ~OFFLOAD_TX_ANY;
    if (!(v & SOLO5_NET_OFFLOAD_RX_CSUM))
        v &= ~SOLO5_NET_OFFLOAD_RX_GRO;

    return v;
}

static uint32_t offload_max_frame(uint32_t v, uint16_t mtu, uint32_t max)
{
    if (v & (OFFLOAD_TX_ANY | SOLO5_NET_OFFLOAD_RX_GRO))
        return max;

    return mtu + 14;
}

int tap_offload_setup(struct tap *d, const char *ifname, uint32_t offloads)
{
    int mtu;

    memset(d, 0, sizeof *d);
    if (offloads == 0) {
        if ((d->fd = tap_attach(ifname, &mtu)) < 0)
            return -1;
        /* A pre-opened fd may already prepend an offload header to each
         * frame; without --net-offload we would hand it to the guest as is. */
        if (ifname[0] == '@' && os_hdr_enabled(d->fd) != 0) {
            warnx("%s: fd has an offload header enabled, use --net-offload or "
                  "reopen it without",
                  ifname);
            errno = EINVAL;
            return -1;
        }
        d->kind = TAP_HDR_NONE;
        d->mtu = mtu;
        d->max_frame = mtu + 14;
        return 0;
    }

    if (os_setup(d, ifname, offloads) == -1)
        return -1;

    d->offloads = offload_normalize(d->offloads & offloads);
    if (d->offloads == 0) {
        close(d->fd);
        errno = ENOTSUP;
        return -1;
    }

    if (d->offloads != offload_normalize(offloads))
        warnx("%s: offloads 0x%x requested, 0x%x supported", ifname, offloads,
              d->offloads);

    d->max_frame = offload_max_frame(d->offloads, d->mtu, d->max_frame);
    return 0;
}

#include <sys/uio.h>

struct vnet_hdr {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
};

_Static_assert(sizeof(struct vnet_hdr) == 12, "struct vnet_hdr is 12 bytes");

#define VNET_F_NEEDS_CSUM 1
#define VNET_F_DATA_VALID 2
#define VNET_GSO_NONE     0
#define VNET_GSO_TCPV4    1
#define VNET_GSO_TCPV6    4
#define VNET_GSO_UDP_L4   5
#define VNET_GSO_ECN      0x80

static uint32_t gso_offload(const struct frame_info *fi)
{
    if (fi->l4proto == 17)
        return SOLO5_NET_OFFLOAD_TX_USO;

    return fi->ipv6 ? SOLO5_NET_OFFLOAD_TX_TSO6 : SOLO5_NET_OFFLOAD_TX_TSO4;
}

ssize_t tap_write(struct tap *d, const uint8_t *buf, size_t len)
{
    struct frame_info fi;
    size_t wlen = len;
    bool big = false;
    uint16_t seg = 0;

    if (d->kind == TAP_HDR_NONE)
        return write(d->fd, buf, len);

    if (len > d->max_frame || frame_classify(buf, len, &fi) == -1)
        goto einval;
    if (fi.eligible) {
        wlen = fi.ip_end; /* drop Ethernet padding, see tap_frame.h */
        big = wlen > (size_t)d->mtu + FRAME_ETH_HLEN;
        if (big) {
            seg = frame_seg(&fi, d->mtu);
            if (seg == 0 || !(d->offloads & gso_offload(&fi)))
                goto einval;
        }
    }
    /* Non-eligible frames go out, the kernel decides. */

    struct vnet_hdr h;
#ifdef TUNSCAP
    struct tun_hdr th;
#endif

    struct iovec iov[2];
    switch (d->kind) {
    case TAP_HDR_VIRTIO:
        memset(&h, 0, sizeof h);
        if (fi.eligible) {
            h.hdr_len = fi.hdr_len;
            if (d->offloads & SOLO5_NET_OFFLOAD_TX_CSUM) {
                h.flags = VNET_F_NEEDS_CSUM;
                h.csum_start = fi.csum_start;
                h.csum_offset = fi.csum_offset;
            }
            if (big) {
                h.gso_type = fi.l4proto == 17 ? VNET_GSO_UDP_L4
                             : fi.ipv6        ? VNET_GSO_TCPV6
                                              : VNET_GSO_TCPV4;
                if (fi.l4proto == 6 && (buf[fi.csum_start + 13] & 0x80))
                    h.gso_type |= VNET_GSO_ECN;
                h.gso_size = seg;
            }
        }
        iov[0].iov_base = &h;
        iov[0].iov_len = sizeof h;
        break;
#ifdef TUNSCAP
    case TAP_HDR_OPENBSD:
        memset(&th, 0, sizeof th);
        /* TODO */
        iov[0].iov_base = &th;
        iov[0].iov_len = sizeof th;
        break;
#endif
    default:
        goto einval;
    }
    iov[1].iov_base = (void *)(uintptr_t)buf;
    iov[1].iov_len = wlen;

    ssize_t n = writev(d->fd, iov, 2);

    if (n == -1)
        return -1;
    if ((size_t)n != iov[0].iov_len + wlen) {
        errno = EIO;
        return -1;
    }
    return (ssize_t)len;

einval:
    errno = EINVAL;
    return -1;
}

#ifndef TUNSCAP
static bool rx_virtio_accept(struct tap *d, const struct vnet_hdr *h,
                             uint8_t *buf, size_t n)
{
    uint8_t gso = h->gso_type & ~VNET_GSO_ECN;

    if (gso != VNET_GSO_NONE &&
        !((gso == VNET_GSO_TCPV4 || gso == VNET_GSO_TCPV6) &&
          (d->offloads & SOLO5_NET_OFFLOAD_RX_GRO)))
        return false;

    if (!(d->offloads & SOLO5_NET_OFFLOAD_RX_CSUM))
        return true;

    struct frame_info fi;
    bool eligible = frame_classify(buf, n, &fi) == 0 && fi.eligible;

    if (h->flags & VNET_F_NEEDS_CSUM) {
        /* The L4 field holds a partial (pseudo-header) sum. An eligible
         * frame can keep it, as the guest will not verify it. */
        if (eligible && h->csum_start == fi.csum_start &&
            h->csum_offset == fi.csum_offset)
            return true;
        /* Otherwise, the guest will verify it: complete it here, at the
         * place the kernel told us. */
        if (frame_csum_complete(buf, n, h->csum_start, h->csum_offset) == -1)
            return false;

        return true;
    }

    /* Already verified by the kernel or the NIC. */
    if (h->flags & VNET_F_DATA_VALID)
        return true;

    /* No information: verify what the guest will not. */
    if (eligible && !frame_l4_csum_ok(buf, &fi))
        return false;

    return true;
}
#endif

ssize_t tap_read(struct tap *d, uint8_t *buf, size_t len)
{
    if (d->kind == TAP_HDR_NONE)
        return read(d->fd, buf, len);

    for (;;) {
#ifdef TUNSCAP
        struct tun_hdr th;
        struct iovec iov[2] = {{&th, sizeof th}, {buf, len}};
#else
        struct vnet_hdr h;
        struct iovec iov[2] = {{&h, sizeof h}, {buf, len}};
#endif
        ssize_t n = readv(d->fd, iov, 2);

        if (n == -1)
            return -1;
        if ((size_t)n < iov[0].iov_len) {
            errno = EIO;
            return -1;
        }
        n -= iov[0].iov_len;

#ifdef TUNSCAP
        return n; /* TODO: OpenBSD */
#else
        if (rx_virtio_accept(d, &h, buf, (size_t)n))
            return n;
#endif
    }
}
