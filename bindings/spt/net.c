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

static struct mft *mft;
static int epollfd;
static int npollfds;

void net_init(struct spt_boot_info *bi)
{
    mft = bi->mft;
    epollfd = bi->epollfd;

    npollfds = 0;
    for (unsigned i = 0; i != mft->entries; i++) {
	if (mft->e[i].type == MFT_NET_BASIC)
	    npollfds++;
    }
}

solo5_result_t solo5_net_acquire(const char *name, solo5_handle_t *handle,
        struct solo5_net_info *info)
{
    unsigned index;
    struct mft_entry *e = mft_get_by_name(mft, name, MFT_NET_BASIC, &index);
    if (e == NULL)
        return SOLO5_R_EINVAL;
    assert(e->attached);

    *handle = index;
    info->mtu = e->u.net_basic.mtu;
    memcpy(info->mac_address, e->u.net_basic.mac,
            sizeof info->mac_address);
    return SOLO5_R_OK;
}

solo5_result_t solo5_net_read(solo5_handle_t handle, uint8_t *buf, size_t size,
	size_t *read_size)
{
    struct mft_entry *e = mft_get_by_index(mft, handle, MFT_NET_BASIC);
    if (e == NULL)
        return SOLO5_R_EINVAL;
    
    long nbytes = sys_read(e->hostfd, (char *)buf, size);
    if (nbytes < 0) {
        if (nbytes == SYS_EAGAIN)
            return SOLO5_R_AGAIN;
        else
            return SOLO5_R_EUNSPEC;
    }

    *read_size = (size_t)nbytes;
    return SOLO5_R_OK;
}

solo5_result_t solo5_net_write(solo5_handle_t handle, const uint8_t *buf,
	size_t size)
{
    struct mft_entry *e = mft_get_by_index(mft, handle, MFT_NET_BASIC);
    if (e == NULL)
        return SOLO5_R_EINVAL;

    long nbytes = sys_write(e->hostfd, (const char *)buf, size);

    return (nbytes == (int)size) ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
}

bool solo5_yield(solo5_time_t deadline, solo5_handle_set_t *ready_set)
{
    uint64_t now, timeout_nsecs;

    now = solo5_clock_monotonic();
    if (deadline <= now)
        timeout_nsecs = 0;
    else
        timeout_nsecs = deadline - now;

    int nrevents;
    /*
     * At least one event must be requested in epoll(), otherwise the call will
     * just return or error.
     */
    int nevents = npollfds ? npollfds : 1;
    struct sys_epoll_event revents[nevents];
    solo5_handle_set_t tmp_ready_set = 0;
    
    /*
     * TODO: This reduces timeout granularity to milliseconds. Use an internal
     * timerfd here?
     */
    do {
        nrevents = sys_epoll_pwait(epollfd, revents, nevents,
                timeout_nsecs / 1000000ULL, NULL, 0);
    } while (nrevents == SYS_EINTR);
    assert(nrevents >= 0);
    if (nrevents > 0) {
        for (int i = 0; i < nrevents; i++)
            tmp_ready_set |= 1ULL << revents[i].data.u64;
    }
    if (ready_set != NULL)
        *ready_set = tmp_ready_set;
    return (nrevents > 0);
}
