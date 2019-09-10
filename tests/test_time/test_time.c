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

#include <stdarg.h>
#include <stddef.h>

#include "solo5.h"
#include "../../bindings/lib.c"
#include "../../bindings/printf.c"

static void printf(const char *fmt, ...)
    __attribute__ ((format (printf, 1, 2)));

static void printf(const char *fmt, ...)
{
    char buffer[1024];
    va_list args;
    size_t size;

    va_start(args, fmt);
    size = vsnprintf(buffer, sizeof buffer, fmt, args);
    va_end(args);

    if (size >= sizeof buffer) {
        const char trunc[] = "(truncated)\n";
        solo5_console_write(buffer, sizeof buffer - 1);
        solo5_console_write(trunc, sizeof trunc - 1);
    }
    else {
        solo5_console_write(buffer, size);
    }
}

#define NSEC_PER_SEC 1000000000ULL

int solo5_app_main(const struct solo5_start_info *si __attribute__((unused)))
{
    printf("\n**** Solo5 standalone test_time ****\n\n");

    /*
     * Verify that monotonic time is passing
     */
    solo5_time_t ta = 0, tb = 0;
    int iters = 5;
    ta = solo5_clock_monotonic();
    tb = solo5_clock_monotonic();
    /*
     * Adjacent calls to solo5_clock_monotonic() may return the same time count
     * on aarch64 systems, so give it a few extra iterations to change.
     */
    while (ta == tb && iters--)
        tb = solo5_clock_monotonic();
    if (!(tb > ta)) {
        printf("ERROR: time is not passing\n");
        return SOLO5_EXIT_FAILURE;
    }

    /* 
     * The tender is configured with no I/O modules for this test so
     * solo5_yield() is equivalent to a sleep here.
     */
    ta = solo5_clock_monotonic();
    solo5_yield(ta + NSEC_PER_SEC, NULL);
    tb = solo5_clock_monotonic();
    /*
     * Verify that we did not sleep less than requested (see above).
     */
    if ((tb - ta) < NSEC_PER_SEC) {
        printf("ERROR: slept too little (%llu)\n",
                (unsigned long long)(tb - ta));
        return SOLO5_EXIT_FAILURE;
    }
    /*
     * Verify that we did not sleep more than requested, within reason
     * (scheduling delays, general inaccuracy of the current timing code).
     */
    if ((tb - ta) > (NSEC_PER_SEC + 100000000ULL)) {
        printf("ERROR: slept too much (%llu)\n",
                (unsigned long long)(tb - ta));
        return SOLO5_EXIT_FAILURE;
    }

    /*
     * Verify that wall time is 2017 or later
     */
    ta = solo5_clock_wall();
    if (ta < (1483228800ULL * NSEC_PER_SEC)) {
        printf("ERROR: wall time is not 2017 or later (%llu)\n",
                (unsigned long long)ta);
        return SOLO5_EXIT_FAILURE;
    }

    printf("SUCCESS\n");
    return SOLO5_EXIT_SUCCESS;
}
