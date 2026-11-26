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
    struct vnet_hdr hdr;

    switch (d->kind) {
    case TAP_HDR_VIRTIO:
        memset(&hdr, 0, sizeof(hdr));
        if (fi.eligible) {
            hdr.hdr_len = fi.hdr_len;
            // CheckSUM Offload
            if (d->offloads & SOLO5_NET_OFFLOAD_TX_CSUM) {
                hdr.flags = VNET_F_NEEDS_CSUM;
                hdr.csum_start = fi.csum_start;
                hdr.csum_offset = fi.csum_offset;
            }
            // TCP Segmentation Offload
            if (big) {
                hdr.gso_type = fi.l4proto == 17 ? VNET_GSO_UDP_L4
                               : fi.ipv6        ? VNET_GSO_TCPV6
                                                : VNET_GSO_TCPV4;
                if (fi.l4proto == 6 && (buf[fi.csum_start + 13] & 0x80))
                    hdr.gso_type |= VNET_GSO_ECN;
                hdr.gso_size = seg;
            }
        }
        iov[0].iov_base = &hdr;
        iov[0].iov_len = sizeof(hdr);
        break;
    case TAP_HDR_OPENBSD:
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

static bool validate(struct tap *d, const struct vnet_hdr *hdr, uint8_t *buf,
                     size_t len)
{
    uint8_t gso = hdr->gso_type & ~VNET_GSO_ECN;

    /* if it's a GSO_TCPV{4,6} frame and we don't handle RX_GRO, we return
     * false. */
    if (gso != VNET_GSO_NONE &&
        !((gso == VNET_GSO_TCPV4 || gso == VNET_GSO_TCPV6) &&
          (d->offloads & SOLO5_NET_OFFLOAD_RX_GRO)))
        return false;

    /* if we don't handle checksum offload, we return true. */
    if (!(d->offloads & SOLO5_NET_OFFLOAD_RX_CSUM))
        return true;

    struct frame_info fi;
    bool eligible = frame_classify(buf, len, &fi) == 0 && fi.eligible;

    if (hdr->flags & VNET_F_NEEDS_CSUM) {
        /* this particular case corresponds to the local network where the
           kernel does **not** calculate the checksum, but it corresponds to a
           UDP/TCP frame (and if it's IPv6, it's without extension headers):
           there is no need to check/complete the checksum (as it never leaves
           the kernel). */
        if (eligible && hdr->csum_start == fi.csum_start &&
            hdr->csum_offset == fi.csum_offset)
            return true;
        /* here, our frame is not eligible: we calculate and write the checksum
           against the value provided by (hdr). */
        if (frame_csum_complete(buf, len, hdr->csum_start, hdr->csum_offset) ==
            -1)
            return false;

        return true;
    }

    /* here, we trust the kernel. */
    if (hdr->flags & VNET_F_DATA_VALID)
        return true;

    /* here, if it's an eligible frame and the checksum is not right, we fail.
     */
    if (eligible && !frame_l4_csum_ok(buf, &fi))
        return false;

    return true;
}

ssize_t tap_read(struct tap *d, uint8_t *buf, size_t len)
{
    if (d->kind == TAP_HDR_NONE)
        return read(d->fd, buf, len);

    for (;;) {
        struct vnet_hdr hdr;
        struct iovec iov[2] = {{&hdr, sizeof(hdr)}, {buf, len}};

        ssize_t res = readv(d->fd, iov, 2);

        if (res == -1)
            return -1;
        if ((size_t)res < iov[0].iov_len) {
            errno = EIO;
            return -1;
        }

        res -= (ssize_t)iov[0].iov_len;

        if (validate(d, &hdr, buf, (size_t)res))
            return res;
    }
}
