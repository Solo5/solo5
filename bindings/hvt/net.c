/*
 * Copyright (c) 2015-2019 Contributors as noted in the AUTHORS file
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

#include "bindings.h"
#include "hvt_ring.h"

static const struct mft *mft;
static struct hvt_ring *net_ring;
static uint32_t ring_req_id;
/* Here, we’re doing things differently from VirtIO, which allocates buffers
 * using `malloc()`. Our buffers will be in the .bss section! */
static uint8_t write_bufs[HVT_RING_SIZE][HVT_RING_BUF_SIZE];

static inline void cpu_relax(void)
{
#if defined(__x86_64__)
    __asm__ __volatile__("pause" ::: "memory");
#elif defined(__aarch64__)
    __asm__ __volatile__("yield" ::: "memory");
#endif
}

/*
 * Submit a request to the ring and kick if needed.
 * Does NOT wait for completion, caller decides whether to wait.
 */
static inline void ring_submit(struct hvt_ring *ring)
{
    /* as VirtIO, we add wmb() here to add a new entry. */
    hvt_wmb();
    ring->ent_tail++;

    /* Kick suppression: only signal the I/O thread via ioeventfd if it has
     * indicated it is about to sleep (needs_kick == 1). When the thread is
     * actively polling, the kick is unnecessary overhead.
     *
     * IMPORTANT: hvt_mb() (mfence on x86) is required here, not just hvt_rmb().
     * We need a Store-Load barrier to ensure our store to ent_tail is globally
     * visible before we load needs_kick. Otherwise on x86, the load of
     * needs_kick can bypass the store to ent_tail (store-buffer forwarding),
     * and both sides can miss each other's updates (which results in a
     * deadlock).
     *
     * As VirtIO, we need to use mb() when we would like to "kick"/NOTIFY the
     * host thread.
     */
    hvt_mb();
    if (ring->needs_kick)
        hvt_ring_kick(0);
}

solo5_result_t solo5_net_write(solo5_handle_t handle, const uint8_t *buf,
                               size_t size)
{
    if (net_ring) {
        /*
         * Flow control: wait if the ring is full. The host advances ent_head
         * after processing each write, freeing slots.
         */
        while ((net_ring->ent_tail - net_ring->ent_head) >= (HVT_RING_SIZE - 1))
            cpu_relax();

        uint32_t idx = net_ring->ent_tail & HVT_RING_MASK;
        struct hvt_ring_entry *ent = &net_ring->entries[idx];

        /* Fall back to hypercall for packets that do not fit in the
         * per-slot buffer (e.g. jumbo frames with MTU > HVT_RING_BUF_SIZE).
         */
        if (size > HVT_RING_BUF_SIZE)
            goto hypercall_write;

        /* Copy data into a per-slot static buffer so the caller can safely
         * reuse its buffer after we return (fire-and-forget). write_bufs[] is
         * in guest BSS (low memory), well below the stack and the ring which
         * are at the top of guest memory.
         */
        memcpy(write_bufs[idx], buf, size);
        ent->operation = HVT_RING_NET_WRITE;
        ent->handle = handle;
        ent->data = (uint64_t)(uintptr_t)write_bufs[idx];
        ent->len = size;
        ent->id = ring_req_id++;
        ring_submit(net_ring);

        /* Fire-and-forget: no completion wait for writes */
        return SOLO5_R_OK;
    }

hypercall_write:;
    volatile struct hvt_hc_net_write wr;

    wr.handle = handle;
    wr.data = buf;
    wr.len = size;
    wr.ret = 0;

    hvt_do_hypercall(HVT_HYPERCALL_NET_WRITE, &wr);

    return wr.ret;
}

/*
 * Wait for the host to post a completion entry.
 */
static inline void ring_wait_commit(struct hvt_ring *ring)
{
    while (ring->com_head == ring->com_tail)
        cpu_relax();
    hvt_rmb();
}

solo5_result_t solo5_net_read(solo5_handle_t handle, uint8_t *buf, size_t size,
                              size_t *read_size)
{
    if (net_ring) {
        /*
         * Each read is synchronous (submit + wait_commit), so ent_head
         * has advanced before the next read. The ring cannot be full here
         * unless there is a bug in the write batching logic.
         */
        assert((net_ring->ent_tail - net_ring->ent_head) < (HVT_RING_SIZE - 1));

        uint32_t idx = net_ring->ent_tail & HVT_RING_MASK;
        struct hvt_ring_entry *ent = &net_ring->entries[idx];

        ent->operation = HVT_RING_NET_READ;
        ent->handle = handle;
        ent->data = (uint64_t)(uintptr_t)buf;
        ent->len = size;
        ent->id = ring_req_id++;

        /* NOTE(dinosaure): it seems that (in terms of performance) it is better
         * to offload frame reading to the host pthread and wait for commit of
         * the operation (and certainly avoid any VM exits under high load) than
         * to make a hypercall.
         *
         * During an iperf3 benchmark, the unikernel appears to be fast enough
         * to overwhelm the host’s pthread (meaning it does not need to be
         * "kicked"). In such a situation, waiting via our ring seems more
         * advantageous than the execution path involving a hypercall.
         */
        ring_submit(net_ring);
        ring_wait_commit(net_ring);

        uint32_t commit_idx = net_ring->com_head & HVT_RING_MASK;
        struct hvt_ring_commit *commit = &net_ring->commits[commit_idx];
        solo5_result_t ret = (solo5_result_t)commit->ret;
        *read_size = commit->len;
        net_ring->com_head++;

        return ret;
    }

    volatile struct hvt_hc_net_read rd;

    rd.handle = handle;
    rd.data = buf;
    rd.len = size;
    rd.ret = 0;

    hvt_do_hypercall(HVT_HYPERCALL_NET_READ, &rd);

    *read_size = rd.len;
    return rd.ret;
}

solo5_result_t solo5_net_acquire(const char *name, solo5_handle_t *handle,
                                 struct solo5_net_info *info)
{
    unsigned index;
    const struct mft_entry *e =
        mft_get_by_name(mft, name, MFT_DEV_NET_BASIC, &index);
    if (e == NULL)
        return SOLO5_R_EINVAL;
    assert(e->attached);

    *handle = index;
    info->mtu = e->u.net_basic.mtu;
    memcpy(info->mac_address, e->u.net_basic.mac, sizeof info->mac_address);
    return SOLO5_R_OK;
}

void net_init(const struct hvt_boot_info *bi)
{
    mft = bi->mft;
    net_ring = NULL;
    ring_req_id = 0;

    if (bi->host_features & HVT_FEATURE_RING_IO) {
        net_ring = (struct hvt_ring *)(uintptr_t)bi->net_ring;
    }
}
