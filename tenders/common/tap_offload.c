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
