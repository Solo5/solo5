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

int frame_classify(const uint8_t *f, size_t len, struct frame_info *fi);
uint16_t frame_seg(const struct frame_info *fi, uint16_t mtu);
uint16_t frame_pseudo_sum(const uint8_t *f, const struct frame_info *fi);
bool frame_l4_csum_ok(const uint8_t *f, const struct frame_info *fi);
int frame_csum_complete(uint8_t *f, size_t len, size_t start, size_t offset);

#endif
