/*
 * Copyright (c) 2017 Contributors as noted in the AUTHORS file
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

#include "../kernel.h"
#include "sinfo.h"
#include "mutimeinfo.h"

static struct time_info_type *time_info;

/* Wall time offset at monotonic time base. */
static uint64_t wc_epochoffset;

/* Base time values at the last call to clock_monotonic(). */
static uint64_t time_base;
static uint64_t tsc_base;

/* Multiplier for converting TSC ticks to nsecs. (0.32) fixed point. */
static uint32_t tsc_mult;

/* TSC value of current minor frame start */
static uint64_t current_start = 0;
static uint64_t min_delta;

uint64_t tscclock_monotonic(void)
{
    const uint64_t next_start = muen_get_sched_start();
    uint64_t tsc_now, tsc_delta;
    tsc_now = tsc_base;

    if (next_start == current_start)
        tsc_now += min_delta;
    else
        tsc_now = current_start = next_start;

    tsc_delta = tsc_now - tsc_base;
    time_base += mul64_32(tsc_delta, tsc_mult);
    tsc_base = tsc_now;

    return time_base;
}

int tscclock_init(uint64_t freq __attribute__((unused)))
{
    struct muen_memregion_info region;
    uint64_t tsc_freq;

    if (!muen_get_memregion_info("time_info", &region)) {
        log(WARN, "Unable to retrieve Muen time info region\n");
        return -1;
    }

    time_info = (struct time_info_type *)region.address;

    wc_epochoffset = 0;
    cc_barrier();

    /* Wait until time information has been published */
    do
    {
        wc_epochoffset = time_info->tsc_time_base * 1000;
        cc_barrier();
    } while (wc_epochoffset == 0);

    tsc_freq = time_info->tsc_tick_rate_hz;
    tsc_mult = (NSEC_PER_SEC << 32) / tsc_freq;
    min_delta = (tsc_freq + (NSEC_PER_SEC - 1)) / NSEC_PER_SEC;
    time_base = 0;
    log(INFO, "Solo5: Clock source: Muen PV clock, TSC frequency %lu Hz\n",
        (unsigned long long)tsc_freq);
    return 0;
}

uint64_t tscclock_epochoffset(void)
{
    return wc_epochoffset;
}
