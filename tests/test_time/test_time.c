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

#include "solo5.h"
#include "../../kernel/lib.c"

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

#define NSEC_PER_SEC 1000000000ULL

int solo5_app_main(char *cmdline __attribute__((unused)))
{
    puts("\n**** Solo5 standalone test_time ****\n\n");

    /*
     * Verify that monotonic time is passing
     */
    uint64_t ta, tb;
    ta = solo5_clock_monotonic();
    tb = solo5_clock_monotonic();
    if (!(tb > ta)) {
        puts("ERROR: time is not passing\n");
        return 1;
    }

    /* 
     * The monitor is configured with no I/O modules for this test so
     * solo5_poll() is equivalent to a sleep here.
     */
    ta = solo5_clock_monotonic();
    solo5_poll(ta + NSEC_PER_SEC);
    tb = solo5_clock_monotonic();
    /*
     * Verify that we did not sleep less than requested (see above).
     */
    if ((tb - ta) < NSEC_PER_SEC) {
        puts("ERROR: slept too little\n");
        return 1;
    }
    /*
     * Verify that we did not sleep more than requested, within reason
     * (scheduling delays, general inaccuracy of the current timing code).
     */
    if ((tb - ta) > (NSEC_PER_SEC + 20000000ULL)) {
        puts("ERROR: slept too much\n");
        return 1;
    }

    /*
     * Verify that wall time is 2017 or later
     */
    ta = solo5_clock_wall();
    if (ta < (1483228800ULL * NSEC_PER_SEC)) {
        puts("ERROR: wall time is not 2017 or later\n");
        return 1;
    }

    puts("SUCCESS\n");
    return 0;
}
