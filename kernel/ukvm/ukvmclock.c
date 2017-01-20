/*-
 * Copyright (c) 2014, 2015 Antti Kantee.  All Rights Reserved.
 * Copyright (c) 2015 Martin Lucina.  All Rights Reserved.
 * Modified for solo5 by Ricardo Koller <kollerr@us.ibm.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "kernel.h"

/* RTC wall time offset at monotonic time base. */
static uint64_t rtc_epochoffset;

/*
 * TSC clock specific.
 */

/* Base time values at the last call to tscclock_monotonic(). */
static uint64_t time_base;
static uint64_t tsc_base;

/* Multiplier for converting TSC ticks to nsecs. (0.32) fixed point. */
static uint32_t tsc_mult;

/*
 * Return monotonic time using TSC clock.
 */
uint64_t ukvmclock_monotonic(void) {
    uint64_t tsc_now, tsc_delta;

    /*
     * Update time_base (monotonic time) and tsc_base (TSC time).
     */
    tsc_now = cpu_rdtsc();
    tsc_delta = tsc_now - tsc_base;
    time_base += mul64_32(tsc_delta, tsc_mult);
    tsc_base = tsc_now;

    return time_base;
}

/*
 * Calibrate TSC and initialise TSC clock.
 */
int ukvmclock_init(void) {
    uint64_t tsc_freq, rtc_boot;
    struct ukvm_time_init t;

    outl(UKVM_PORT_TIME_INIT, ukvm_ptr(&t));
    cc_barrier();

    /*
     * Read RTC "time at boot". This must be done just before tsc_base is
     * initialised in order to get a correct offset below.
     */
    rtc_boot = t.rtc_boot;
    tsc_freq = t.freq;
    tsc_base = cpu_rdtsc();
    printf("Solo5: Clock source: ukvm, frequency estimate is %lu Hz\n",
           (unsigned long long)tsc_freq);

    /*
     * Calculate TSC scaling multiplier.
     *
     * (0.32) tsc_mult = NSEC_PER_SEC (32.32) / tsc_freq (32.0)
     */
    tsc_mult = (NSEC_PER_SEC << 32) / tsc_freq;

    /*
     * Monotonic time begins at tsc_base (first read of TSC before
     * calibration).
     */
    time_base = mul64_32(tsc_base, tsc_mult);

    /*
     * Compute RTC epoch offset by subtracting monotonic time_base from RTC
     * time at boot.
     */
    rtc_epochoffset = rtc_boot - time_base;

    return 0;
}

/*
 * Return epoch offset (wall time offset to monotonic clock start).
 */
uint64_t ukvmclock_epochoffset(void) {
	return rtc_epochoffset;
}

