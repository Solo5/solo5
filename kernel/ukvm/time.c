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
#include "ukvm.h"

/* sleep for given seconds */
int sleep(uint32_t seconds) {
    struct ukvm_nanosleep t;
    int dbg = 0;

    t.sec_in = seconds;
    t.nsec_in = 0;
    outl(UKVM_PORT_NANOSLEEP, ukvm_ptr(&t));
    cc_barrier();

    dprintf("nanosleep = {%d %ld %ld} ns %ld\n",
            t.ret, t.sec_out, t.nsec_out, pvclock_monotonic());

    if ( t.ret < 0 )
        return t.sec_out;

    return 0;
}

/* sleep for given seconds */
void sleep_test(void) {
    int i;
    for (i = 0; i < 5; i++) {
        printf("%d ", i);
        sleep(i);
    }
}


void time_init(void) {
    pvclock_init();
}

uint64_t solo5_clock_monotonic(void)
{
    return pvclock_monotonic();
}
