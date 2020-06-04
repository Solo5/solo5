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

/*
 * bindings.h: Solo5 bindings, Xen implementation additions.
 *
 * This header file includes (supersedes) the common bindings.h for the Xen
 * implementation.
*/

#ifndef __XEN_BINDINGS_H__
#define __XEN_BINDINGS_H__

#include "../bindings.h"
#include "elf_abi.h"

#include "xen/xen.h"
#include "xen/event_channel.h"

/* console.c: Xen console */
void console_init(void);
void console_write(const char *, size_t);

/* pvclock.c: Xen paravirtualized clock */
int pvclock_init(void);
uint64_t pvclock_monotonic(void);
uint64_t pvclock_epochoffset(void);

/* time.c: Timekeeping */
void time_init(void);

/* evtchn.c: Xen event channel interface */
typedef void(*evtchn_handler_fn_t)(evtchn_port_t, void *);

void evtchn_init(void);
void evtchn_register_handler(evtchn_port_t, evtchn_handler_fn_t, void *);
evtchn_port_t evtchn_bind_virq(uint32_t);
void evtchn_mask(evtchn_port_t);
void evtchn_unmask(evtchn_port_t);

/* Accessors for Xen shared_info and VCPU 0 info structures shared with
 * hypervisor.
 */
extern uint8_t HYPERVISOR_SHARED_INFO[];

inline struct shared_info *SHARED_INFO(void)
{
    return (struct shared_info *)&HYPERVISOR_SHARED_INFO;
}

inline struct vcpu_info *VCPU0_INFO(void)
{
    return &(SHARED_INFO()->vcpu_info[0]);
}

#endif /* __XEN_BINDINGS_H__ */
