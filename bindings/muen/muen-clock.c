/*
 * Copyright (c) 2017 Contributors as noted in the AUTHORS file
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
#include "sinfo.h"
#include "mutimeinfo.h"

static volatile struct time_info_type *time_info;

/* Base time values at the last call to clock_monotonic(). */
static uint64_t time_base;
static uint64_t tsc_base;

/* Multiplier for converting TSC ticks to nsecs. (0.32) fixed point. */
static uint32_t tsc_mult;

/* TSC value of current minor frame start */
static uint64_t current_start = 0;
static uint64_t min_delta;

solo5_time_t solo5_clock_monotonic(void)
{
    const uint64_t next_start = muen_get_sched_start();
    uint64_t tsc_now, tsc_delta;
    tsc_now = tsc_base;

    if (next_start == current_start)
        tsc_now += min_delta;
    else
        tsc_now = current_start = next_start;

    tsc_delta = tsc_now - tsc_base;
    time_base += mul64_32(tsc_delta, tsc_mult, 32);
    tsc_base = tsc_now;

    return time_base;
}

void time_init(void)
{
    uint64_t tsc_freq, tsc_time_base;
    const struct muen_resource_type *const
        region = muen_get_resource("time_info", MUEN_RES_MEMORY);

    if (!region) {
        log(ERROR, "Unable to retrieve Muen time info region\n");
        return;
    }

    time_info = (struct time_info_type *)region->data.mem.address;

    tsc_time_base = 0;
    cc_barrier();

    /* Wait until time information has been published */
    do
    {
        tsc_time_base = time_info->tsc_time_base;
        cc_barrier();
    } while (tsc_time_base == 0);

    tsc_freq = time_info->tsc_tick_rate_hz;
    /*
     * TODO: This calculation may overflow for low values of tsc_freq;
     * dynamically calculate tsc_shift as in hvt version.
     */
    tsc_mult = (NSEC_PER_SEC << 32) / tsc_freq;
    min_delta = (tsc_freq + (NSEC_PER_SEC - 1)) / NSEC_PER_SEC;
    time_base = 0;
    log(INFO, "Solo5: Clock source: Muen PV clock, TSC frequency %llu Hz\n",
        (unsigned long long)tsc_freq);
}

uint64_t tscclock_epochoffset(void)
{
    const struct time_info_type ti = *time_info;

    return ti.tsc_time_base * 1000;
}

solo5_time_t solo5_clock_wall(void)
{
    solo5_time_t timestamp;

    timestamp = tscclock_epochoffset();
    if (timestamp != 0) {
        timestamp += solo5_clock_monotonic();
    }

    return timestamp;
}
