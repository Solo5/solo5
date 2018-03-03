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

/*
 * The virtio target uses the KVM paravirtualized clock for timekeeping if
 * available, otherwise the TSC is used. CPU blocking-when-idle is performed
 * using the PIT via cpu_block() in tscclock.c.
 */
static int use_pvclock;

/* return ns since time_init() */
solo5_time_t solo5_clock_monotonic(void)
{
    if (use_pvclock)
        return pvclock_monotonic();
    else
        return tscclock_monotonic();
}

/* return wall time in nsecs */
solo5_time_t solo5_clock_wall(void)
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

bool solo5_yield(solo5_time_t deadline)
{
    bool rc = false;

    /*
     * cpu_block() as currently implemented will only poll for the maximum time
     * the PIT can be run in "one shot" mode. Loop until either I/O is possible
     * or the desired time has been reached.
     */
    cpu_intr_disable();
    do {
        if (virtio_net_pkt_poll()) {
            rc = true;
            break;
        }

        cpu_block(deadline);
    } while (solo5_clock_monotonic() < deadline);
    if (!rc)
        rc = virtio_net_pkt_poll();
    cpu_intr_enable();

    return rc;
}
