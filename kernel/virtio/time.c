/* Copyright (c) 2015, IBM 
 * Author(s): Dan Williams <djwillia@us.ibm.com> 
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


/*
 * Unit definitions:
 *
 * Ticks -  This is defined as a "tick" of the oscillator in the
 *          Intel 8253 Programmable Interval Timer (PIT).                            
 *          The oscillator is fixed by the hardware to run at about
 *          1193182 ticks per second  (or, 1.193182 MHZ)
 *
 * Counts - Unit where every "count" an IRQ0 will be fired.  It is 
 *          defined such that there are 100 ticks in a count, 
 *          for ease of mathematics. The number of ticks per count 
 *          is also the number to set the reload count of the PIT to.
 *
 *
 * MS     - Milliseconds
 *
 */


static int use_pvclock = 0;
static volatile uint64_t counts_since_startup = 0;

/* return ns since time_init() */
uint64_t solo5_clock_monotonic(void) {
    if (use_pvclock)
        return pvclock_monotonic();
    else
        return tscclock_monotonic();
}

/* return wall time in nsecs */
uint64_t solo5_clock_wall(void) {
    return solo5_clock_monotonic() + cpu_clock_epochoffset();
}

/* sleep for given seconds */
int sleep(uint32_t secs) {
    uint64_t curr = solo5_clock_monotonic();
    uint64_t until = curr + NSEC_PER_SEC * (uint64_t) secs;

    interrupts_disable();

    /* cpu_block will enable and check for interrupts */
    while (until > solo5_clock_monotonic())
        cpu_block(until);

    interrupts_enable();

    return 0;
}

void solo5_cpu_block(uint64_t until_nsecs)
{
    cpu_block(until_nsecs);
}

/* called on whenever the PIT fires (i.e. IRQ0 fires) */
void increment_time_count(void) {
    counts_since_startup++;
}

// TODO(DanB): Perhaps choose a better name?
// Name is a bit long
uint64_t time_counts_since_startup(void) {
    return counts_since_startup;
}

/* must be called before interrupts are enabled */
void time_init(void) {
    use_pvclock = !pvclock_init();

    if (!use_pvclock) {
        tscclock_init();
    }

    i8254_init();
}

void sleep_test(void) {
    int i;
    for(i = 0; i < 5; i++) {
        uint64_t tsc = rdtsc();
        sleep(5);
        printf("Timer freq: %u\n", rdtsc() - tsc);
    }
}
