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

/*
 * tap_frame.h: Ethernet frame classification and Internet checksums used
 * by the offload-aware TAP I/O path.
 *
 * With offloads enabled, the host kernel exchanges frames with a
 * (virtio_net_hdr) whose meaning depends on the frame's L3/L4 layout
 * ((csum_start), (csum_offset), (hdr_len), (gso_size)). The unikernel, however,
 * only sees plain Ethernet frames. These helpers let the tender:
 * - on TX, derive the (virtio_net_hdr) fields from the frame itself, so the
 *   guest never has to provide them
 * - on RX, normalise what the kernel hands us (partial checksums, GRO
 *   aggregates) into a frame the guest can consume as-is.
 *
 * A frame is "eligible" for offloads iff:
 * - EtherType is IPv4 (0x0800) or IPv6 (0x86dd)
 * - IPv4: not fragmented (MF = 0, offset = 0), IHL >= 5;
 * - IPv6: Next Header is directly TCP or UDP (no extension header);
 * - L4 is TCP (6) or UDP (17) and its header fgits in the IP payload.
 *
 * Ethernet padding: a frame may be longer than 14 + IP total length (minimum
 * frame size padding). (ip_end) marks the real end of the IP datagram; bytes
 * past it are not covered by any checksum and must not be passed to the kernel
 * together with a GSO header.
 */

#ifndef COMMON_TAP_FRAME_H
#define COMMON_TAP_FRAME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define FRAME_ETH_HLEN 14

struct frame_info {
    bool eligible; /* frame matches the solo5.h eligibility rule */
    bool ipv6;
    uint8_t l4proto; /* 6 (TCP) or 17 (UDP) when eligible */
    uint8_t ip_hlen; /* 20..60 (IPv4), 60 (IPv6) */
    uint8_t l4_hlen; /* 20..60 (TCP), 8 (UDP) */
    uint16_t csum_start; /* offset of the L4 header */
    uint16_t csum_offset; /* offset of the checksum field from csum_start */
    uint16_t hdr_len; /* Ethernet + IP + L4 headers */
    uint32_t ip_end; /* 14 + IP total length; <= frame length */
};

/*
 * Parses the Ethernet frame (f) of (len) bytes and fills (*fi).
 *
 * Returns 0 and sets (fi->eligible) when the frame is an eligible TCP/UDP frame
 * (see above); all other fields of (*fi) are then valid.
 *
 * Returns 0 with (fi->eligible == false) for any frame outside the scope of
 * offloads (non-IP, fragment, other L4, ...): such a frame must be forwarded
 * unmodified.
 *
 * Returns -1 when the frame claims to be TCP/UDP over IP bit its lengths are
 * inconsistent (malformed): the caller must drop it.
 *
 * (*fi) is always zeroed first.
 */
int frame_classify(const uint8_t *f, size_t len, struct frame_info *fi);

/*
 * Returns the GSO (General Segmentation Offload) segment size (payload bytes
 * per segment) for an eligible frame given the device (mtu) (excluding Ethernet
 * header), or 0 if the IP + L4 headers do not fit in (mtu).
 */
uint16_t frame_seg(const struct frame_info *fi, uint16_t mtu);

/*
 * Returns the folded, NOT complemented, one's-complement sum of the TCP/UDP
 * pseudo-header of eligible frame (f). This is the value the L4 checksum field
 * must hold when VNET_F_NEEDS_CSUM is set.
 */
uint16_t frame_pseudo_sum(const uint8_t *f, const struct frame_info *fi);

/*
 * Verifies the L4 checksum of eligible frame (f) over [csum_start, ip_end). A
 * zero UDP checksum means "no checksum": accepted on IPv4, rejected on IPv6.
 * It ensures the SOLO5_OFFLOAD_RX_CSUM promise to the guest when the kernel
 * gave no checksum status.
 */
bool frame_l4_csum_ok(const uint8_t *f, const struct frame_info *fi);

/*
 * Completes a partial checksum as requested by VNET_F_NEEDS_CSUM: sums
 * (f)[start..len), complements it and stores it big-endian at
 * (f)[start + offset]. A result of 0 is written as 0xffff (RFC 768). Does not
 * rely on frame_classify(), so it works on any frame layout the kernel
 * describes. Returns -1 if (start, offset) fall outside (len).
 */
int frame_csum_complete(uint8_t *f, size_t len, size_t start, size_t offset);

#endif
