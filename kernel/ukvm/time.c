/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a unikernel base layer.
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

#include "kernel.h"

void time_init(uint64_t tsc_freq)
{
    assert(tscclock_init(tsc_freq) == 0);
}

uint64_t solo5_clock_monotonic(void)
{
    return tscclock_monotonic();
}

/* return wall time in nsecs */
uint64_t solo5_clock_wall(void)
{
    return tscclock_monotonic() + tscclock_epochoffset();
}

int solo5_poll(uint64_t until_nsecs)
{
    struct ukvm_poll t;
    uint64_t now;

    now = solo5_clock_monotonic();
    if (until_nsecs <= now)
        t.timeout_nsecs = 0;
    else
        t.timeout_nsecs = until_nsecs - now;
    ukvm_do_hypercall(UKVM_HYPERCALL_POLL, &t);
    return t.ret;
}
