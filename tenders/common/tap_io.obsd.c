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

#include <sys/socket.h>
#include <sys/uio.h>
#include <net/if.h>
#include <net/if_tun.h>

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
        wlen = fi.ip_end; /* drop Ethernet padding */
        big = wlen > (size_t)d->mtu + SOLO5_NET_HLEN;
        if (big) {
            seg = frame_seg(&fi, d->mtu);
            /* if we don't have, at least, one segment or if the recognized
             * required offload from the given (buf) is not supported by our tap
             * device. */
            if (seg == 0 || !(d->offloads & gso_offload(&fi)))
                goto einval;
        }
    }

    struct iovec iov[2];
    struct tun_hdr th;

    switch (d->kind) {
    case TAP_HDR_OPENBSD:
        memset(&th, 0, sizeof(th));
        if (fi.eligible && (d->offloads & SOLO5_NET_OFFLOAD_TX_CSUM))
            th.th_flags = fi.l4proto == 6 ? TUN_H_TCP_CSUM : TUN_H_UDP_CSUM;
        if (big) {
            th.th_flags |= TUN_H_TCP_MSS;
            th.th_mss = seg;
        }
        iov[0].iov_base = &th;
        iov[0].iov_len = sizeof(th);
        break;
    case TAP_HDR_VIRTIO:
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

static bool validate(struct tap *d, const struct tun_hdr *th, uint8_t *buf,
                     size_t len)
{
    uint16_t l4 =
        th->th_flags & (TUN_H_TCP_CSUM | TUN_H_UDP_CSUM | TUN_H_ICMP_CSUM);
    struct frame_info fi;

    if (th->th_flags & TUN_H_VTAG) /* VLAN 802.1Q */
        return false;
    /* we never ask IPv4 checksum offload, so we should never have
     * TUN_H_IPV4_CSUM. */
    if (th->th_flags & TUN_H_IPV4_CSUM)
        return false;
    /* check if we handle GRO if OpenBSD gives TCP_MSS. */
    if ((th->th_flags & TUN_H_TCP_MSS) &&
        !(d->offloads & SOLO5_NET_OFFLOAD_RX_GRO))
        return false;
    if (frame_classify(buf, len, &fi) == -1)
        return false;

    /* the frame carries a complete checksum. we verify it if we negotiated with
     * the guest RX_CSUM (which means that the guest will not verify it). */
    if (l4 == 0) {
        if (th->th_flags & TUN_H_TCP_MSS)
            return false;
        if (fi.eligible && (d->offloads & SOLO5_NET_OFFLOAD_RX_CSUM) &&
            !frame_l4_csum_ok(buf, &fi))
            return false;
        return true;
    }

    if (l4 == TUN_H_ICMP_CSUM || !fi.eligible ||
        (l4 == TUN_H_TCP_CSUM && fi.l4proto != 6) ||
        (l4 == TUN_H_UDP_CSUM && fi.l4proto != 17))
        return false;

    /* OpenBSD did not compute the checksum and we agreed with the guest that it
     * should not verify the checksum. */
    if (d->offloads & SOLO5_NET_OFFLOAD_RX_CSUM)
        return true;

    return frame_csum_complete(buf, fi.ip_end, fi.csum_start, fi.csum_offset) ==
           0;
}

ssize_t tap_read(struct tap *d, uint8_t *buf, size_t len)
{
    if (d->kind == TAP_HDR_NONE)
        return read(d->fd, buf, len);

    for (;;) {
        struct tun_hdr th;
        struct iovec iov[2] = {{&th, sizeof(th)}, {buf, len}};

        ssize_t res = readv(d->fd, iov, 2);

        if (res == -1)
            return -1;
        if ((size_t)res < iov[0].iov_len) {
            errno = EIO;
            return -1;
        }

        res -= (ssize_t)iov[0].iov_len;

        if (validate(d, &th, buf, (size_t)res))
            return res;
    }
}
