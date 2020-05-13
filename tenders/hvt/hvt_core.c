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

/*
 * hvt_core.c: Core functionality.
 *
 * Maintains tables of modules, hypercall handlers and vmexit handlers.
 * Implements core hypercall functionality which is always present.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if defined(__linux__)

#include <sys/epoll.h>
#include <sys/timerfd.h>

#elif defined(__FreeBSD__) || defined(__OpenBSD__)

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#else

#error Unsupported target

#endif

#include "hvt.h"

hvt_hypercall_fn_t hvt_core_hypercalls[HVT_HYPERCALL_MAX] = { 0 };

int hvt_core_register_hypercall(int nr, hvt_hypercall_fn_t fn)
{
    if (nr >= HVT_HYPERCALL_MAX)
        return -1;
    if (hvt_core_hypercalls[nr] != NULL)
        return -1;

    hvt_core_hypercalls[nr] = fn;
    return 0;
}

#define HVT_HALT_HOOKS_MAX 8
hvt_halt_fn_t hvt_core_halt_hooks[HVT_HALT_HOOKS_MAX] = {0};
static int nr_halt_hooks;

int hvt_core_register_halt_hook(hvt_halt_fn_t fn)
{
    if (nr_halt_hooks == HVT_HALT_HOOKS_MAX)
        return -1;

    hvt_core_halt_hooks[nr_halt_hooks] = fn;
    nr_halt_hooks++;
    return 0;
}

int hvt_core_hypercall_halt(struct hvt *hvt, hvt_gpa_t gpa)
{
    void *cookie;
    int idx;
    struct hvt_hc_halt *t =
            HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_hc_halt));

    /*
     * If the guest set a non-NULL cookie (non-zero before conversion), verify
     * that the memory space pointed to by it is accessible and pass it down to
     * halt hooks, if any.
     */
    if (t->cookie != 0)
        cookie = HVT_CHECKED_GPA_P(hvt, t->cookie, HVT_HALT_COOKIE_MAX);
    else
        cookie = NULL;

    for (idx = 0; idx < nr_halt_hooks; idx++) {
        hvt_halt_fn_t fn = hvt_core_halt_hooks[idx];
        assert(fn != NULL);
        fn(hvt, t->exit_status, cookie);
    }

    return t->exit_status;
}

#define NUM_MODULES 8
hvt_vmexit_fn_t hvt_core_vmexits[NUM_MODULES + 1] = { 0 };
static int nvmexits = 0;

int hvt_core_register_vmexit(hvt_vmexit_fn_t fn)
{
    if (nvmexits == NUM_MODULES)
        return -1;

    hvt_core_vmexits[nvmexits] = fn;
    nvmexits++;
    return 0;
}

static void hypercall_walltime(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_hc_walltime *t =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_hc_walltime));
    struct timespec ts;

    int rc = clock_gettime(CLOCK_REALTIME, &ts);
    assert(rc == 0);
    t->nsecs = (ts.tv_sec * 1000000000ULL) + ts.tv_nsec;
}

static void hypercall_puts(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_hc_puts *p =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_hc_puts));
    int rc = write(1, HVT_CHECKED_GPA_P(hvt, p->data, p->len), p->len);
    assert(rc >= 0);
}

static int waitsetfd = -1;
static int npollfds;
#if defined(__linux__)
static int timerfd = -1;
#define INTERNAL_TIMERFD (~1U)
#endif

static void setup_waitset(void)
{
#if defined(__linux__)
    waitsetfd = epoll_create1(0);
    if (waitsetfd == -1)
        err(1, "Could not create wait set");

    timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerfd == -1)
        err(1, "Could not create wait set timerfd");

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.u64 = INTERNAL_TIMERFD;
    if (epoll_ctl(waitsetfd, EPOLL_CTL_ADD, timerfd, &ev) == -1)
        err(1, "epoll_ctl(EPOLL_CTL_ADD) failed");
#else /* kqueue */
    waitsetfd = kqueue();
    if (waitsetfd == -1)
        err(1, "Could not create wait set");
#endif
}

int hvt_core_register_pollfd(int fd, uintptr_t waitset_data)
{
    if (waitsetfd == -1)
        setup_waitset();

#if defined(__linux__)
    struct epoll_event ev;
    ev.events = EPOLLIN;
    /*
     * waitset_data is a solo5_handle_t, and will be returned by epoll() as
     * part of any received event.
     */
    ev.data.u64 = waitset_data;
    if (epoll_ctl(waitsetfd, EPOLL_CTL_ADD, fd, &ev) == -1)
        err(1, "epoll_ctl(EPOLL_CTL_ADD) failed");
#else /* kqueue */
    struct kevent ev;
    /*
     * waitset_data is a solo5_handle_t, and will be returned by kevent() as
     * part of any received event.
     */
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, (void *)waitset_data);
    if (kevent(waitsetfd, &ev, 1, NULL, 0, NULL) == -1)
        err(1, "kevent(EV_ADD) failed");
#endif
    npollfds++;
    return 0;
}

static void hypercall_poll(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_hc_poll *t =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_hc_poll));

#if defined(__linux__)
    /*
     * On Linux, in order to support nanosecond timeouts, as defined by the
     * Solo5 API, we use a timerfd internally in the waitset. Account for this
     * in the number of requested events.
     */
    int nevents = npollfds ? (npollfds + 1) : 1;
    int nrevents;
    uint64_t ready_set = 0;

    struct epoll_event revents[nevents];
    struct itimerspec it = {
        .it_interval = { 0 },
        .it_value = {
            .tv_sec = t->timeout_nsecs / 1000000000ULL,
            .tv_nsec = t->timeout_nsecs % 1000000000ULL
        }
    };
    /*
     * Ensure that it.it_value is always non-zero, otherwise the following
     * epoll_wait() will hang if there are no other descriptors in the waitset
     * due to the timer never firing. See timefd_settime(2).
     */
    it.it_value.tv_nsec |= 1;
    if (timerfd_settime(timerfd, 0, &it, NULL) == -1)
        err(1, "timerfd_settime() failed");
    /*
     * We can always safely restart this call on EINTR, since the internal
     * timerfd is independent of its invocation.
     */
    do {
        nrevents = epoll_pwait(waitsetfd, revents, nevents, -1, NULL);
    } while (nrevents == -1 && errno == EINTR);
    if (nrevents > 0) {
        int orig_nrevents = nrevents;
        for (int i = 0; i < orig_nrevents; i++)
            if (revents[i].data.u64 == INTERNAL_TIMERFD)
                nrevents -= 1;          /* Disregard in total reported events */
            else
                ready_set |= (1ULL << revents[i].data.u64);
    }
    assert(nrevents >= 0);
#else /* kqueue */
    /*
     * At least one event must be requested in kevent(), otherwise the call
     * will just return or error.
     */
    int nevents = npollfds ? npollfds : 1;
    int nrevents;
    uint64_t ready_set = 0;
    struct kevent revents[nevents];
    struct timespec ts;

    ts.tv_sec = t->timeout_nsecs / 1000000000ULL;
    ts.tv_nsec = t->timeout_nsecs % 1000000000ULL;

    nrevents = kevent(waitsetfd, NULL, 0, revents, nevents, &ts);
    /*
     * Unlike the epoll() implementation, we can't easily restart the kqueue()
     * call on EINTR, due to not having a straightforward way to recalculate
     * the timeout.  While we could use EVFILT_TIMER similarly to the Linux
     * timerfd, this has system-wide limits on the number of active timers.
     *
     * However: We don't handle any signals, other than by terminating the
     * tender.  Therefore, we should never see EINTR in practice here. If this
     * turns out not to be the case, prominently warn the user about it and
     * pretend we woke up early with no events, which is better than just
     * asserting/aborting.
     */
    if (nrevents == -1 && errno == EINTR) {
        warnx("hypercall_poll(): kqueue() returned EINTR");
        warnx("hypercall_poll(): This should not happen, please report a bug");
        nrevents = 0;
    }
    assert(nrevents >= 0);
    if (nrevents > 0) {
        for (int i = 0; i < nrevents; i++)
            ready_set |= (1ULL << (uintptr_t)revents[i].udata);
    }
#endif
    t->ready_set = ready_set;
    t->ret = nrevents;
}

static int setup(struct hvt *hvt, struct mft *mft)
{
    if (waitsetfd == -1)
        setup_waitset();

    assert(hvt_core_register_hypercall(HVT_HYPERCALL_WALLTIME,
                hypercall_walltime) == 0);
    assert(hvt_core_register_hypercall(HVT_HYPERCALL_PUTS,
                hypercall_puts) == 0);
    assert(hvt_core_register_hypercall(HVT_HYPERCALL_POLL,
                hypercall_poll) == 0);

    return 0;
}

DECLARE_MODULE(core,
    .setup = setup
)
