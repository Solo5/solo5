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

/* must be called before interrupts are enabled */
void time_init(void)
{
    use_pvclock = !pvclock_init();

    if (!use_pvclock)
        assert(tscclock_init() == 0);
}

int solo5_poll(uint64_t until_nsecs, short *events, short *revents)
{
    int rc = 0;
    int net_event = 0;

    int idx_first_net = solo5_get_first_netiface()->poll_event_idx;

    /*
     * cpu_block() as currently implemented will only poll for the maximum time
     * the PIT can be run in "one shot" mode. Loop until either I/O is possible
     * or the desired time has been reached.
     */
    interrupts_disable();
    do {
        net_event = (events[idx_first_net] & SOLO5_POLLIN)
                    && virtio_net_pkt_poll();
        if (net_event) {
            rc = 1;
            break;
        }

        cpu_block(until_nsecs);
    } while (solo5_clock_monotonic() < until_nsecs);
    if (!rc) {
        net_event = (events[idx_first_net] & SOLO5_POLLIN)
                    && virtio_net_pkt_poll();
        rc = net_event;
    }
    interrupts_enable();

    revents[idx_first_net] = net_event;

    return rc;
}
