/*
 * Copyright (c) 2026 Contributors as noted in the AUTHORS file
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
 * hvt_ring.h: Shared ring buffer definitions for ioeventfd-base I/O.
 *
 * This header file is used by both the tender (host) and the guest (bindings).
 */

#ifndef HVT_RING_H
#define HVT_RING_H

#include <stdint.h>

#define HVT_RING_SIZE 1024
#define HVT_RING_MASK (HVT_RING_SIZE - 1)
#define HVT_RING_BUF_SIZE                                                      \
    2048 /* per-slot data buffer (>= max ethernet frame) */

/*
 * Number of spin iterations the I/O thread performs before blocking on
 * the eventfd. Tuned for ~1-2ns of spinning at typical CPU frequencies.
 */
#define HVT_RING_POLL_ITERS 4096

/*
 * Ring operations.
 */
#define HVT_RING_NET_WRITE 1
#define HVT_RING_NET_READ  2

/*
 * Submission entry: written by the guest, consumed by the host.
 */
struct hvt_ring_entry {
    uint32_t operation;
    uint32_t handle;
    uint64_t data; /* GPA of the data buffer */
    uint32_t len;
    uint32_t id;
    uint64_t _reserved;
};

/*
 * Commit entry: written by the host, consumed by the guest.
 */
struct hvt_ring_commit {
    uint32_t id;
    int32_t ret;
    uint32_t len;
    uint32_t _reserved;
};

/*
 * Shared ring structure. Indices are separated into distinct cache lines to
 * avoid false sharing between guest and host.
 *
 * Cache line 0: written ONLY by the guest (ent_tail, com_head)
 * Cache line 1: written ONLY by thes host (ent_head, com_tail, needs_kick)
 *
 * This ensures each side only writes to its own cache line, eliminating
 * false sharing bounces.
 */
struct hvt_ring {
    /* Cache line 0: written ONLY by the guest */
    volatile uint32_t ent_tail; /* produced by guest */
    volatile uint32_t com_head; /* consumed by guest */
    uint8_t _pad0[56];

    /* Cache line 1: written ONLY by the host */
    volatile uint32_t ent_head; /* consumed by host */
    volatile uint32_t com_tail; /* produced by host */
    volatile uint32_t needs_kick; /* set by host before sleeping */
    uint8_t _pad1[52];

    struct hvt_ring_entry entries[HVT_RING_SIZE];
    struct hvt_ring_commit commits[HVT_RING_SIZE];
};

/*
 * Memory barriers.
 *
 * hvt_wmb() / hvt_rmb(): used for the SPSC ring indices ([ent_tail],
 * [com_tail]). On x86 TSO, stores are not re-ordered with sotrees and loads
 * are not re-ordered with loads, so a compiler barrier suffices.
 *
 * hvt_mb(): full Store-Load barrier. Required for the [needs_kick] protocol
 * where both sides do "store mine; load theirs". x86 TSO allows a younger load
 * to bypass an older store to a different address (stored-buffer forwarding),
 * so [mfence] is needed here.
 *
 * On aarch64 (weakly ordered): real fence instructions are always required.
 */
#ifdef __x86_64__
static inline void hvt_wmb(void)
{
    __asm__ __volatile__("" ::: "memory");
}

static inline void hvt_rmb(void)
{
    __asm__ __volatile__("" ::: "memory");
}

static inline void hvt_mb(void)
{
    __asm__ __volatile__("mfence" ::: "memory");
}
#elif defined(__aarch64__)
static inline void hvt_wmb(void)
{
    __asm__ __volatile__("dmb ishst" ::: "memory");
}

static inline void hvt_rmb(void)
{
    __asm__ __volatile__("dmb ishld" ::: "memory");
}

static inline void hvt_mb(void)
{
    __asm__ __volatile__("dmb ish" ::: "memory");
}
#else
#error Unsupported architecture
#endif

#endif /* HVT_RING_H */
