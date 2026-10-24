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

#ifndef COMMON_TAP_OFFLOAD_H
#define COMMON_TAP_OFFLOAD_H

#include <stdint.h>

enum tap_hdr_kind { TAP_HDR_NONE, TAP_HDR_VIRTIO, TAP_HDR_OPENBSD };

struct tap {
    int fd;
    enum tap_hdr_kind kind;
    uint32_t offloads; /* SOLO5_NET_OFFLOAD_* effectively enabled */
    uint16_t mtu;
    uint32_t max_frame;
};

/*
 * Open (ifname) (or "@NN") and configure the offloads in (offloads).
 * On success, (d->offloads) is a subset of (offloads), normalized so that
 * TSO/USO imply TX_CSUM and GRO implies RX_CSUM. Returns -1 and errno on
 * failure. Must be called before any sandboxing (pledge, capsicum).
 */
int tap_offload_setup(struct tap *d, const char *ifname, uint32_t offloads);

#endif
