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

#define MS_PER_SEC 1000 /* milliseconds per second */
#define NS_PER_SEC 1000000000 /* ns per second */

static volatile uint64_t sec_since_startup = 0;

/* return ms since time_init() */
uint64_t time_monotonic_ms(void) {
    return (uint64_t) (sec_since_startup * 
        MS_PER_SEC);
}

/* return ns since time_init() */
uint64_t time_monotonic_ns(void) {
    return (uint64_t) (sec_since_startup * 
        NS_PER_SEC);
}


/* sleep for given seconds */
void sleep(uint32_t seconds) {
    uint64_t wakeup = sec_since_startup + seconds;

    /* wait until its time to wake up */
    while (sec_since_startup < wakeup)
        ;
}


void increment_time_secs(void) {
    sec_since_startup++;
}
