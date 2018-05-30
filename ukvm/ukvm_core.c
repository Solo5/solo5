/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of ukvm, a unikernel monitor.
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
 * ukvm_core.c: Core functionality.
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

#include "ukvm.h"

struct ukvm_module ukvm_module_core;

struct ukvm_module *ukvm_core_modules[] = {
    &ukvm_module_core,
    &ukvm_module_dumpcore,
#ifdef UKVM_MODULE_BLK
    &ukvm_module_blk,
#endif
#ifdef UKVM_MODULE_NET
    &ukvm_module_net,
#endif
#ifdef UKVM_MODULE_GDB
    &ukvm_module_gdb,
#endif
    NULL,
};
#define NUM_MODULES ((sizeof ukvm_core_modules / sizeof (struct ukvm_module *)) - 1)

ukvm_hypercall_fn_t ukvm_core_hypercalls[UKVM_HYPERCALL_MAX] = { 0 };

int ukvm_core_register_hypercall(int nr, ukvm_hypercall_fn_t fn)
{
    if (nr >= UKVM_HYPERCALL_MAX)
        return -1;
    if (ukvm_core_hypercalls[nr] != NULL)
        return -1;

    ukvm_core_hypercalls[nr] = fn;
    return 0;
}

ukvm_shutdown_fn_t ukvm_core_shutdown_hooks[UKVM_SHUTDOWN_HOOKS_MAX] = {0};
static int nr_sd_hooks;

int ukvm_core_register_shutdown_hook(ukvm_shutdown_fn_t fn)
{
    if (nr_sd_hooks == UKVM_SHUTDOWN_HOOKS_MAX)
        return -1;

    ukvm_core_shutdown_hooks[nr_sd_hooks] = fn;
    nr_sd_hooks++;
    return 0;
}

int ukvm_core_hypercall_halt(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    void *cookie;
    int idx;
    struct ukvm_halt *t =
            UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_halt));

    /* A NULL pointer in the unikernel should be a NULL in the tender. */
    if (t->cookie)
	/*
         * It is extremely important that all shutdown hooks using cookie do
         * not access more than UKVM_COOKIE_MAX bytes of cookie as anything
         * above that address could be above the memory of the guest.
         */
        cookie = UKVM_CHECKED_GPA_P(hv, t->cookie, UKVM_COOKIE_MAX);
    else
        cookie = NULL;

    for (idx = 0; idx < nr_sd_hooks; idx++) {
        ukvm_shutdown_fn_t fn = ukvm_core_shutdown_hooks[idx];
        assert(fn != NULL);
        fn(hv, t->exit_status, cookie);
    }

    return t->exit_status;
}

ukvm_vmexit_fn_t ukvm_core_vmexits[NUM_MODULES + 1] = { 0 };
static int nvmexits = 0;

int ukvm_core_register_vmexit(ukvm_vmexit_fn_t fn)
{
    if (nvmexits == NUM_MODULES)
        return -1;

    ukvm_core_vmexits[nvmexits] = fn;
    nvmexits++;
    return 0;
}

static void hypercall_walltime(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    struct ukvm_walltime *t =
        UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_walltime));
    struct timespec ts;

    int rc = clock_gettime(CLOCK_REALTIME, &ts);
    assert(rc == 0);
    t->nsecs = (ts.tv_sec * 1000000000ULL) + ts.tv_nsec;
}

static void hypercall_puts(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    struct ukvm_puts *p =
        UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_puts));
    int rc = write(1, UKVM_CHECKED_GPA_P(hv, p->data, p->len), p->len);
    assert(rc >= 0);
}

static struct pollfd pollfds[NUM_MODULES];
static int npollfds = 0;
static sigset_t pollsigmask;

int ukvm_core_register_pollfd(int fd)
{
    if (npollfds == NUM_MODULES)
        return -1;

    pollfds[npollfds].fd = fd;
    pollfds[npollfds].events = POLLIN;
    npollfds++;
    return 0;
}

static void hypercall_poll(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    struct ukvm_poll *t =
        UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_poll));
    struct timespec ts;
    int rc;

    ts.tv_sec = t->timeout_nsecs / 1000000000ULL;
    ts.tv_nsec = t->timeout_nsecs % 1000000000ULL;

    rc = ppoll(pollfds, npollfds, &ts, &pollsigmask);
    assert(rc >= 0);
    t->ret = rc;
}

static int setup(struct ukvm_hv *hv)
{
    assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_WALLTIME,
                hypercall_walltime) == 0);
    assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_PUTS,
                hypercall_puts) == 0);
    assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_POLL,
                hypercall_poll) == 0);

    /*
     * XXX: This needs documenting / coordination with the top-level caller.
     */
    sigfillset(&pollsigmask);
    sigdelset(&pollsigmask, SIGTERM);
    sigdelset(&pollsigmask, SIGINT);

    return 0;
}

struct ukvm_module ukvm_module_core = {
    .name = "core",
    .setup = setup
};
