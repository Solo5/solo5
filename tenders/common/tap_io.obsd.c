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
#include "tap_io.h"

#include <sys/socket.h>
#include <sys/uio.h>
#include <net/if.h>
#include <net/if_tun.h>

#define IPPROTO_UDP 17
#define IPPROTO_TCP 6

bool tx_fill_offload_hdr(struct tap *d, const struct frame_info fi,
                         const uint8_t *buf, size_t len, uint8_t *raw)
{
    struct tun_hdr *th = (struct tun_hdr *)raw;

    if (d->kind != TAP_HDR_OPENBSD)
        return false;

    memset(th, 0, sizeof(*th));
    if (fi.eligible && (d->offloads & SOLO5_NET_OFFLOAD_TX_CSUM))
        th->th_flags =
            fi.l4proto == IPPROTO_TCP ? TUN_H_TCP_CSUM : TUN_H_UDP_CSUM;
    if (fi.eligible && fi.ip_end > (size_t)d->mtu + SOLO5_NET_HLEN) {
        th->th_flags |= TUN_H_TCP_MSS;
        th->th_mss = frame_seg(&fi, d->mtu);
    }

    return true;
}

bool rx_validate_hdr(struct tap *d, const uint8_t *raw, uint8_t *buf,
                     size_t len)
{
    const struct tun_hdr *th = (const struct tun_hdr *)raw;
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
        (l4 == TUN_H_TCP_CSUM && fi.l4proto != IPPROTO_TCP) ||
        (l4 == TUN_H_UDP_CSUM && fi.l4proto != IPPROTO_UDP))
        return false;

    /* OpenBSD did not compute the checksum and we agreed with the guest that it
     * should not verify the checksum. */
    if (d->offloads & SOLO5_NET_OFFLOAD_RX_CSUM)
        return true;

    return frame_csum_complete(buf, fi.ip_end, fi.csum_start, fi.csum_offset) ==
           0;
}
