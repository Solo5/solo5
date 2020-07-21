/*
 * Copyright (c) 2015-2020 Contributors as noted in the AUTHORS file
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

#include "bindings.h"
#include "hypercall.h"

static volatile bool timer_fired;

/* Return ns since unspecified time before time_init(). */
solo5_time_t solo5_clock_monotonic(void)
{
    return pvclock_monotonic();
}

/* Return wall time in nsecs. */
solo5_time_t solo5_clock_wall(void)
{
    return pvclock_monotonic() + pvclock_epochoffset();
}

/*
 * Called in interrupt context.
 */
static void
time_event_handler(evtchn_port_t port __attribute__((unused)),
        void *arg __attribute__((unused)))
{
    timer_fired = true;
}

void solo5_yield(solo5_time_t deadline, solo5_handle_set_t *ready_set)
{
    if (pvclock_monotonic() < deadline) {
        timer_fired = false;
        hypercall_set_timer_op(deadline);
        /*
         * Wait for timer event to fire. On PVH, this is performed by halting
         * the VCPU and waiting for an interrupt (event upcall). As we are only
         * interested in timer events here, we go back into halt state if the
         * interrupt taken was not a timer event.
         *
         * Note that on x86, using HLT to wait for an interrupt is only safe
         * when immediately preceeded by STI and interrupts have been disabled
         * beforehand, otherwise we can take the interrupt before the HLT is
         * executed and go to sleep forever.
         *
         * Further, order is important here; we must check that the timer
         * event has not fired immediately before going into halt state.
         */
        while(true) {
            __asm__ __volatile__ ("cli" : : : "memory");
            if (timer_fired) {
                __asm__ __volatile__ ("sti");
                break;
            }
            else {
                __asm__ __volatile__ ("sti; hlt");
            }
        }
    }
    if (ready_set)
        *ready_set = 0;
}

void time_init(void)
{
    assert(pvclock_init() == 0);

    evtchn_port_t port = evtchn_bind_virq(VIRQ_TIMER);
    evtchn_register_handler(port, time_event_handler, 0);
    evtchn_unmask(port);
}
