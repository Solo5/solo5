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

#ifndef __XEN_HYPERCALL_H__
#define __XEN_HYPERCALL_H__

#include "bindings.h"

#if defined(__x86_64__)
#include "hypercall-x86_64.h"
#define HYPERCALL1 _hypercall64_1
#define HYPERCALL2 _hypercall64_2
#define HYPERCALL3 _hypercall64_3
#define HYPERCALL4 _hypercall64_4
#else
#error Not implemented
#endif

#include "xen/event_channel.h"
#include "xen/hvm/hvm_op.h"
#include "xen/memory.h"
#include "xen/sched.h"
#include "xen/vcpu.h"
#include "xen/xen.h"

/*
 * Hypercall primitives.
 */
static inline long hypercall__memory_op(unsigned int cmd, void *arg)
{
    return HYPERCALL2(long, __HYPERVISOR_memory_op, cmd, arg);
}

static inline long hypercall__sched_op(unsigned int cmd, void *arg)
{
    return HYPERCALL2(long, __HYPERVISOR_sched_op, cmd, arg);
}

static inline long hypercall__event_channel_op(unsigned int cmd, void *arg)
{
    return HYPERCALL2(long, __HYPERVISOR_event_channel_op, cmd, arg);
}

static inline long hypercall__hvm_op(unsigned int cmd, void *arg)
{
    return HYPERCALL2(long, __HYPERVISOR_hvm_op, cmd, arg);
}

/*
 * Higher level hypercall helpers, type-safe.
 */
static inline int hypercall_evtchn_bind_virq(uint32_t virq, uint32_t vcpu,
        evtchn_port_t *port)
{
    evtchn_bind_virq_t op = {
        .virq = virq,
        .vcpu = vcpu
    };
    int rc = hypercall__event_channel_op(EVTCHNOP_bind_virq, &op);

    if (rc == 0)
        *port = op.port;
    return rc;
}

static inline int hypercall_evtchn_send(evtchn_port_t port)
{
    return hypercall__event_channel_op(EVTCHNOP_send, &port);
}

static inline void hypercall_evtchn_unmask(evtchn_port_t port)
{
    evtchn_unmask_t op = {
        .port = port
    };

    (void)hypercall__event_channel_op(EVTCHNOP_unmask, &op);
}

static inline int hypercall_hvm_get_param(unsigned int idx, uint64_t *value)
{
    xen_hvm_param_t p = {
        .domid = DOMID_SELF,
        .index = idx
    };
    int rc = hypercall__hvm_op(HVMOP_get_param, &p);

    if (rc == 0)
        *value = p.value;
    return rc;
}

static inline int hypercall_hvm_set_param(unsigned int idx, uint64_t value)
{
    xen_hvm_param_t p = {
        .domid = DOMID_SELF,
        .index = idx,
        .value = value
    };

    return hypercall__hvm_op(HVMOP_set_param, &p);
}

static inline int hypercall_physmap_add_shared_info(unsigned int idx,
        uint64_t gpfn)
{
    xen_add_to_physmap_t add = {
        .domid = DOMID_SELF,
        .space = XENMAPSPACE_shared_info,
        .idx = idx,
        .gpfn = gpfn
    };

    return hypercall__memory_op(XENMEM_add_to_physmap, &add);
}

static inline int hypercall_set_evtchn_upcall_vector(uint32_t vcpu,
        uint8_t vector)
{
    xen_hvm_evtchn_upcall_vector_t op = {
        .vcpu = vcpu,
        .vector = vector
    };

    return hypercall__hvm_op(HVMOP_set_evtchn_upcall_vector, &op);
}

static inline int hypercall_set_timer_op(uint64_t deadline)
{
    return HYPERCALL1(long, __HYPERVISOR_set_timer_op, deadline);
}

static inline void hypercall_shutdown(unsigned int reason)
{
    (void)hypercall__sched_op(SCHEDOP_shutdown, &reason);
}

static inline void hypercall_yield(void)
{
    (void)hypercall__sched_op(SCHEDOP_yield, NULL);
}

#endif /* __XEN_HYPERCALL_H__ */
