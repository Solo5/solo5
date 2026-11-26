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

#ifndef COMMON_VIRTIO_NET_H
#define COMMON_VIRTIO_NET_H

/* This header comes first in the scatter-gather list.
 * If VIRTIO_F_ANY_LAYOUT is not negotiated, it must
 * be the first element of the scatter-gather list.  If you don't
 * specify GSO or CSUM features, you can simply ignore the header.
 */
struct __attribute__((__packed__)) virtio_net_hdr {
#define VIRTIO_NET_HDR_F_NEEDS_CSUM 1 /* Use csum_start, csum_offset */
#define VIRTIO_NET_HDR_F_DATA_VALID 2 /* Csum is valid */
    uint8_t flags;
#define VIRTIO_NET_HDR_GSO_NONE   0 /* Not a GSO frame */
#define VIRTIO_NET_HDR_GSO_TCPV4  1 /* GSO frame, IPv4 TCP (TSO) */
#define VIRTIO_NET_HDR_GSO_UDP    3 /* GSO frame, IPv4 UDP (UFO) */
#define VIRTIO_NET_HDR_GSO_TCPV6  4 /* GSO frame, IPv6 TCP */
#define VIRTIO_NET_HDR_GSO_UDP_L4 5 /* */
#define VIRTIO_NET_HDR_GSO_ECN    0x80 /* TCP has ECN set */
    uint8_t gso_type;
    uint16_t hdr_len; /* Ethernet + IP + tcp/udp hdrs */
    uint16_t gso_size; /* Bytes to append to hdr_len per frame */
    uint16_t csum_start; /* Position to start checksumming from */
    uint16_t csum_offset; /* Offset after that to place checksum */
};

_Static_assert(sizeof(struct virtio_net_hdr) == 10,
               "struct virtio_net_hdr is 10 bytes");

#endif /* COMMON_VIRTIO_NET_H */
