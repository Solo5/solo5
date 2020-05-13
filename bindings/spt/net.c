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

static const struct mft *mft;
static int epollfd;
static int npollfds;
static int timerfd;

void net_init(struct spt_boot_info *bi)
{
    mft = bi->mft;
    epollfd = bi->epollfd;
    timerfd = bi->timerfd;

    npollfds = 0;
    for (unsigned i = 0; i != mft->entries; i++) {
        if (mft->e[i].type == MFT_DEV_NET_BASIC)
            npollfds++;
    }
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
    memcpy(info->mac_address, e->u.net_basic.mac,
            sizeof info->mac_address);
    return SOLO5_R_OK;
}

solo5_result_t solo5_net_read(solo5_handle_t handle, uint8_t *buf, size_t size,
        size_t *read_size)
{
    const struct mft_entry *e =
        mft_get_by_index(mft, handle, MFT_DEV_NET_BASIC);
    if (e == NULL)
        return SOLO5_R_EINVAL;

    long nbytes = sys_read(e->b.hostfd, (char *)buf, size);
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
    const struct mft_entry *e =
        mft_get_by_index(mft, handle, MFT_DEV_NET_BASIC);
    if (e == NULL)
        return SOLO5_R_EINVAL;

    long nbytes = sys_write(e->b.hostfd, (const char *)buf, size);

    return (nbytes == (int)size) ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
}

void solo5_yield(solo5_time_t deadline, solo5_handle_set_t *ready_set)
{
    int nrevents;
    /*
     * In order to support nanosecond timeouts, as defined by the Solo5 API, we
     * use a timerfd internally in the epoll() set. Account for this in the
     * number of requested events.
     */
    int nevents = npollfds ? (npollfds + 1) : 1;
    struct sys_epoll_event revents[nevents];
    solo5_handle_set_t tmp_ready_set = 0;
    struct sys_itimerspec it = {
        .it_interval = { 0 },
        .it_value = {
            .tv_sec = deadline / 1000000000ULL,
            .tv_nsec = deadline % 1000000000ULL
        }
    };
    /*
     * On spt, given that Solo5 monotonic time is identical to CLOCK_MONOTONIC,
     * we can just pass the deadline into the timerfd as an absolute timeout,
     * saving a clock_gettime() call in the process.
     */
    assert(sys_timerfd_settime(timerfd, SYS_TFD_TIMER_ABSTIME, &it, NULL) != -1);
    /*
     * We can always safely restart this call on EINTR, since the internal
     * timerfd is independent of its invocation.
     */
    do {
        nrevents = sys_epoll_pwait(epollfd, revents, nevents, -1, NULL, 0);
    } while (nrevents == SYS_EINTR);
    if (nrevents > 0) {
        int orig_nrevents = nrevents;
        for (int i = 0; i < orig_nrevents; i++)
            if (revents[i].data == SPT_INTERNAL_TIMERFD)
                nrevents -= 1;          /* Disregard in total reported events */
            else
                tmp_ready_set |= 1ULL << revents[i].data;
    }
    assert(nrevents >= 0);
    if (ready_set != NULL)
        *ready_set = tmp_ready_set;
}
