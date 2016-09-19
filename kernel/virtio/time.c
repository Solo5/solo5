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
 * The virtio target uses the KVM paravirtualized clock for timekeeping if
 * available, otherwise the TSC is used. CPU blocking-when-idle is performed
 * using the PIT via cpu_block() in tscclock.c.
 */
static int use_pvclock;

/* return ns since time_init() */
uint64_t solo5_clock_monotonic(void)
{
    if (use_pvclock)
        return pvclock_monotonic();
    else
        return tscclock_monotonic();
}

/* return wall time in nsecs */
uint64_t solo5_clock_wall(void)
{
    if (use_pvclock)
        return pvclock_monotonic() + pvclock_epochoffset();
    else
        return tscclock_monotonic() + tscclock_epochoffset();
}

static int timer_handler(void *arg __attribute__((unused)))
{
    /* Yes, we handled the irq. */
    return 1;
}

/* must be called before interrupts are enabled */
void time_init(void)
{
    intr_register_irq(0, timer_handler, NULL);
    use_pvclock = !pvclock_init();

    if (!use_pvclock)
        assert(tscclock_init() == 0);
}

int solo5_poll(uint64_t until_nsecs)
{
    int rc = 0;

    /*
     * cpu_block() as currently implemented will only poll for the maximum time
     * the PIT can be run in "one shot" mode. Loop until either I/O is possible
     * or the desired time has been reached.
     */
    intr_disable();
    do {
        if (virtio_net_pkt_poll()) {
            rc = 1;
            break;
        }

        cpu_block(until_nsecs);
    } while (solo5_clock_monotonic() < until_nsecs);
    if (!rc)
        rc = virtio_net_pkt_poll();
    intr_enable();

    return rc;
}
