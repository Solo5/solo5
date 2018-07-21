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

/* Wall clock offset at monotonic time base. */
static uint64_t wc_epochoffset;

/* Base time values at the last call to tscclock_monotonic(). */
static uint64_t time_base;
static uint64_t tsc_base;

/*
 * Shift factor for TSC scaling multiplier; referred to as S in the following
 * comments.
 */
static uint8_t tsc_shift;

/* Multiplier for converting TSC ticks to nsecs. (0.S) fixed point. */
static uint32_t tsc_mult;

#if defined(__x86_64__)
#define READ_CPU_TICKS cpu_rdtsc
#elif defined(__aarch64__)
#define READ_CPU_TICKS cpu_cntvct
#else
#error Unsupported architecture
#endif

/*
 * Beturn monotonic time using TSC clock.
 */
uint64_t tscclock_monotonic(void)
{
    uint64_t tsc_now, tsc_delta;

    /*
     * Update time_base (monotonic time) and tsc_base (TSC time).
     */
    tsc_now = READ_CPU_TICKS();
    tsc_delta = tsc_now - tsc_base;
    time_base += mul64_32(tsc_delta, tsc_mult, tsc_shift);
    tsc_base = tsc_now;

    return time_base;
}

/*
 * Initialise TSC clock.
 *
 * Implementation notes: This is a purely TSC-based clock with the following
 * requirements:
 *
 * 1. The host TSC MUST be invariant, as defined in Intel SDM section 17.15.1
 * "Invariant TSC".
 * 2. The host hypervisor MUST NOT implement any RDTSC emulation.
 *
 * It is up to the monitor to ensure that these requirements are met, and to
 * supply the TSC frequency to the guest.
 */
int tscclock_init(uint64_t tsc_freq)
{
    /*
     * Calculate TSC shift factor and scaling multiplier.
     *
     * tsc_shift (S) needs to be the largest (<=32) shift factor where the
     * result of the tsc_mult calculcation below fits into uint32_t without
     * truncation. Note that we disallow an S of zero to ensure the loop always
     * terminates.
     *
     * (0.S) tsc_mult = NSEC_PER_SEC (S.S) / tsc_freq (S.0)
     */
    tsc_shift = 32;
    uint64_t tmp;
    do {
        tmp = (NSEC_PER_SEC << tsc_shift) / tsc_freq;
        if ((tmp & 0xFFFFFFFF00000000L) == 0L)
            tsc_mult = (uint32_t)tmp;
        else
            tsc_shift--;
    } while (tsc_shift > 0 && tsc_mult == 0L);
    assert(tsc_mult != 0L);
    log(DEBUG, "Solo5: tscclock_init(): tsc_freq=%lu tsc_mult=%lu tsc_shift=%u\n",
        tsc_freq, tsc_mult, tsc_shift);

    /*
     * Monotonic time begins at tsc_base (first read of TSC before
     * calibration).
     */
    tsc_base = READ_CPU_TICKS();
    time_base = mul64_32(tsc_base, tsc_mult, tsc_shift);

    /*
     * Compute wall clock epoch offset by subtracting monotonic time_base from
     * wall time at boot.
     *
     * TODO: This arrangement minimises the use of hypercalls, but is subject
     * to clock skew over time and cannot get corrections from the host (via
     * e.g. NTP). 
     */
    struct ukvm_walltime t;
    ukvm_do_hypercall(UKVM_HYPERCALL_WALLTIME, &t);
    wc_epochoffset = t.nsecs - time_base;

    return 0;
}

/*
 * Return epoch offset (wall time offset to monotonic clock start).
 */
uint64_t tscclock_epochoffset(void)
{
	return wc_epochoffset;
}
