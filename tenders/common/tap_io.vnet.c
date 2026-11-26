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
#include <sys/uio.h>

#include "solo5.h"
#include "tap_attach.h"
#include "tap_offload.h"
#include "tap_frame.h"
#include "tap_io.h"
#include "virtio_net.h"

#define IPPROTO_UDP 17
#define IPPROTO_TCP 6
#define TCP_FLAGS   13
#define TH_CWR      0x80 /* TCP Header: Congestion Window Reduced */

bool tx_fill_offload_hdr(struct tap *d, const struct frame_info fi,
                         const uint8_t *buf, size_t len, uint8_t *raw)
{
    size_t wlen = (fi.eligible) ? fi.ip_end : len; /* drop Ethernet padding */
    struct virtio_net_hdr *hdr = (struct virtio_net_hdr *)raw;

    if (d->kind != TAP_HDR_VIRTIO)
        return false;

    memset(hdr, 0, sizeof(*hdr));
    if (fi.eligible) {
        hdr->hdr_len = fi.hdr_len;
        // CheckSUM Offload
        if (d->offloads & SOLO5_NET_OFFLOAD_TX_CSUM) {
            hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
            hdr->csum_start = fi.csum_start;
            hdr->csum_offset = fi.csum_offset;
        }
        // TCP Segmentation Offload
        if (wlen > (size_t)d->mtu + SOLO5_NET_HLEN) {
            hdr->gso_type = fi.l4proto == IPPROTO_UDP
                                ? VIRTIO_NET_HDR_GSO_UDP_L4
                            : fi.ipv6 ? VIRTIO_NET_HDR_GSO_TCPV6
                                      : VIRTIO_NET_HDR_GSO_TCPV4;
            // If ECN flag is set, set it also on the virtio_net_hdr
            if (fi.l4proto == IPPROTO_TCP &&
                (buf[fi.csum_start + TCP_FLAGS] & TH_CWR))
                hdr->gso_type |= VIRTIO_NET_HDR_GSO_ECN;
            hdr->gso_size = frame_seg(&fi, d->mtu);
        }
    }

    return true;
}

bool rx_validate_hdr(struct tap *d, const uint8_t *raw, uint8_t *buf,
                     size_t len)
{
    const struct virtio_net_hdr *hdr = (const struct virtio_net_hdr *)raw;
    uint8_t gso = hdr->gso_type & ~VIRTIO_NET_HDR_GSO_ECN;

    /* if it's a GSO_TCPV{4,6} frame and we don't handle RX_GRO, we return
     * false. */
    if (gso != VIRTIO_NET_HDR_GSO_NONE &&
        !((gso == VIRTIO_NET_HDR_GSO_TCPV4 ||
           gso == VIRTIO_NET_HDR_GSO_TCPV6) &&
          (d->offloads & SOLO5_NET_OFFLOAD_RX_GRO)))
        return false;

    /* if we don't handle checksum offload, we return true. */
    if (!(d->offloads & SOLO5_NET_OFFLOAD_RX_CSUM))
        return true;

    struct frame_info fi;
    bool csum_known = frame_classify(buf, len, &fi) == 0 && fi.eligible;

    if (hdr->flags & VIRTIO_NET_HDR_F_NEEDS_CSUM) {
        /* this particular case corresponds to the local network where the
           kernel does **not** calculate the checksum, but it corresponds to a
           UDP/TCP frame (and if it's IPv6, it's without extension headers):
           there is no need to check/complete the checksum (as it never leaves
           the kernel). */
        if (csum_known && hdr->csum_start == fi.csum_start &&
            hdr->csum_offset == fi.csum_offset)
            return true;
        /* here, our frame is not TSO/USO: we calculate and write the checksum
           against the value provided by (hdr). */
        if (frame_csum_complete(buf, len, hdr->csum_start, hdr->csum_offset) ==
            -1)
            return false;

        return true;
    }

    /* here, we trust the kernel. */
    if (hdr->flags & VIRTIO_NET_HDR_F_DATA_VALID)
        return true;

    /* here, if it's an eligible frame and the checksum is not right, we fail.
     */
    if (csum_known && !frame_l4_csum_ok(buf, &fi))
        return false;

    return true;
}
