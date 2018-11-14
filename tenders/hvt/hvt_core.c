/*
 * Copyright (c) 2015-2018 Contributors as noted in the AUTHORS file
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
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "hvt.h"

struct hvt_module hvt_module_core;

struct hvt_module *hvt_core_modules[] = {
    &hvt_module_core,
#ifdef HVT_MODULE_DUMPCORE
    &hvt_module_dumpcore,
#endif
#ifdef HVT_MODULE_BLK
    &hvt_module_blk,
#endif
#ifdef HVT_MODULE_NET
    &hvt_module_net,
#endif
#ifdef HVT_MODULE_GDB
    &hvt_module_gdb,
#endif
    NULL,
};
#define NUM_MODULES ((sizeof hvt_core_modules / sizeof (struct hvt_module *)) - 1)

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
    struct hvt_halt *t =
            HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_halt));

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
    struct hvt_walltime *t =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_walltime));
    struct timespec ts;

    int rc = clock_gettime(CLOCK_REALTIME, &ts);
    assert(rc == 0);
    t->nsecs = (ts.tv_sec * 1000000000ULL) + ts.tv_nsec;
}

static void hypercall_puts(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_puts *p =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_puts));
    int rc = write(1, HVT_CHECKED_GPA_P(hvt, p->data, p->len), p->len);
    assert(rc >= 0);
}

#ifndef HVT_MODULE_NET
static void hypercall_noop(struct hvt *hvt, hvt_gpa_t gpa)
{
}
#endif

static struct pollfd pollfds[NUM_MODULES];
static poll_fn_cb_t poll_fn_cb[NUM_MODULES];
static int npollfds = 0;
static sigset_t pollsigmask;

int hvt_core_register_pollfd(int fd, poll_fn_cb_t cb)
{
    if (npollfds == NUM_MODULES)
        return -1;

    pollfds[npollfds].fd = fd;
    pollfds[npollfds].events = POLLIN;

    if (cb) {
        poll_fn_cb[npollfds] = cb;
    }
    npollfds++;
    return 0;
}

static void hypercall_poll(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_poll *t =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_poll));
    struct timespec ts;
    int rc, i;

    ts.tv_sec = t->timeout_nsecs / 1000000000ULL;
    ts.tv_nsec = t->timeout_nsecs % 1000000000ULL;

    rc = ppoll(pollfds, npollfds, &ts, &pollsigmask);
    assert(rc >= 0);

    if (rc) {
        for (i = 0; i < npollfds; i++) {
            if (poll_fn_cb[i] != NULL) {
                poll_fn_cb[i]();
            }
        }
    }
    t->ret = rc;
}

static int setup(struct hvt *hvt)
{
    assert(hvt_core_register_hypercall(HVT_HYPERCALL_WALLTIME,
                hypercall_walltime) == 0);
    assert(hvt_core_register_hypercall(HVT_HYPERCALL_PUTS,
                hypercall_puts) == 0);
    assert(hvt_core_register_hypercall(HVT_HYPERCALL_POLL,
                hypercall_poll) == 0);

    /* HACK!: I need to to register this only in the net module for now. 
     * This will be called by net_init() to setup the kernel's view of shared memory.
     * However net_init() is called every time the kernel is booted, even when net module is 
     * not compiled in. This will result in unknown hyercall number error.
     * I would prefer if there is a way to net init only when net module is required.
     * I guess that's not happening so this is least ugliest hack I could think of */
#ifndef HVT_MODULE_NET
    assert(hvt_core_register_hypercall(HVT_HYPERCALL_SHMINFO,
                hypercall_noop) == 0);
#endif

    /*
     * XXX: This needs documenting / coordination with the top-level caller.
     */
    sigfillset(&pollsigmask);
    sigdelset(&pollsigmask, SIGTERM);
    sigdelset(&pollsigmask, SIGINT);

    return 0;
}

struct hvt_module hvt_module_core = {
    .name = "core",
    .setup = setup
};
