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
#include "xen/hvm/params.h"

/*
 * Find first bit set in (word). Undefined if (word) is zero.
 */
static inline unsigned long ffs(unsigned long word)
{
    return __builtin_ffsl(word) - 1;
}

#define EVTCHN_PORT_MAX 31

struct evtchn_handler {
    evtchn_handler_fn_t handler;
    void *arg;
};

static struct evtchn_handler evtchn_handlers[EVTCHN_PORT_MAX + 1];

void evtchn_register_handler(evtchn_port_t port, evtchn_handler_fn_t handler,
        void *arg)
{
    assert(port <= EVTCHN_PORT_MAX);
    assert(evtchn_handlers[port].handler == NULL);
    cpu_intr_disable();
    evtchn_handlers[port] = (struct evtchn_handler){
        .handler = handler,
        .arg = arg
    };
    cpu_intr_enable();
}

evtchn_port_t evtchn_bind_virq(uint32_t virq)
{
    evtchn_port_t port;

    int rc = hypercall_evtchn_bind_virq(virq, 0, &port);
    assert (rc == 0);
    return port;
}

void evtchn_mask(evtchn_port_t port)
{
    struct shared_info *s = SHARED_INFO();
    atomic_sync_bts(port, &s->evtchn_mask[0]);
}

void evtchn_unmask(evtchn_port_t port)
{
    struct shared_info *s = SHARED_INFO();
    int pending = 0;

    atomic_sync_btc(port, &s->evtchn_mask[0]);
    pending = sync_bt(port, &s->evtchn_pending[0]);
    if (pending) {
        /*
         * Slow path:
         *
         * If pending is set here, then there was a race, and we lost the
         * upcall.  Mask the port again and force an upcall via a call to
         * hyperspace.
         *
         * This should be sufficient for HVM/PVHv2 based on my understanding of
         * Linux drivers/xen/events/events_2l.c.
         */
        atomic_sync_bts(port, &s->evtchn_mask[0]);
        hypercall_evtchn_unmask(port);
    }
}

/*
 * Called in interrupt context.
 */
static int evtchn_vector_handler(void *arg __attribute__((unused)))
{
    struct shared_info *s = SHARED_INFO();
    struct vcpu_info *vi = VCPU0_INFO();

    vi->evtchn_upcall_pending = 0;

    /*
     * Demux events received from Xen.
     *
     * pending_l1 is the "outer" per-VCPU selector (evtchn_pending_sel).
     * pending_l2 is the "inner" system-wide word (evtchn_pending[l1i]).
     */
    xen_ulong_t pending_l1, pending_l2;
    atomic_sync_xchg(&vi->evtchn_pending_sel, 0, &pending_l1);
    while (pending_l1 != 0) {
        xen_ulong_t l1i = ffs(pending_l1);

        /*
         * Masking pending_l2 with ~evtchn_mask[l1i] is necessary to get the
         * *current* masked events; otherwise races like the following
         * can occur:
         *
         *     1. X is generated, upcall scheduled by Xen.
         *     2. X is masked.
         *     3. Upcall is delivered.
         *     4. X fires despite now being masked.
         *
         * Notably, this also applies to events generated during early boot,
         * before evtchn_init() is called (especially from the console driver).
         *
         * Re-loading pending_l2 on each iteration of the inner loop ensures
         * that we continue processing events if new ones are triggered while
         * we're in the loop.
         */
        while ((pending_l2 =
                    (s->evtchn_pending[l1i] & ~s->evtchn_mask[l1i])) != 0) {
            xen_ulong_t l2i = ffs(pending_l2);

            evtchn_port_t port = (l1i * (sizeof(xen_ulong_t) * 8)) + l2i;
            if (port <= EVTCHN_PORT_MAX && evtchn_handlers[port].handler)
                evtchn_handlers[port].handler(port, evtchn_handlers[port].arg);
            else
                log(ERROR, "Solo5: unhandled event on port 0x%x\n", port);

            atomic_sync_btc(l2i, &s->evtchn_pending[l1i]);
        }

        pending_l1 &= ~(1UL << l1i);
    }

    return 1;
}

/*
 * Private ABI to allow override of the event channel vector handler
 * by Mirage/Xen. May change or go away without warning.
 *
 * Note that if you override this handler, then solo5_yield() will no longer
 * work.
 */
int solo5__xen_evtchn_vector_handler(void *)
__attribute__((alias("evtchn_vector_handler"), weak));

void evtchn_init(void)
{
    struct shared_info *s = SHARED_INFO();

    /*
     * Start with all event channels masked.
     */
    for(unsigned e = 0; e < EVTCHN_2L_NR_CHANNELS; e++)
        atomic_sync_bts(e, &s->evtchn_mask[0]);

    /*
     * Register to receive event channel upcalls from Xen via IPI vector #32 which
     * corresponds to IRQ 0 as understood by intr.c.
     */
    intr_register_irq(0, solo5__xen_evtchn_vector_handler, NULL);
    int rc = hypercall_set_evtchn_upcall_vector(0, 32);
    assert(rc == 0);
    /*
     * See init_evtchn() in Xen xen/arch/x86/guest/xen/xen.c.
     */
    rc = hypercall_hvm_set_param(HVM_PARAM_CALLBACK_IRQ, 1);
    assert(rc == 0);
}
