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
 * Based on XTF hypercall_page.S, which is:
 *
 * The Xen Test Framework is licenced under the BSD 2-clause license
 *
 * Copyright (c) 2014,2015 Citrix Systems Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "../cpu_x86_64.h"
#define __ASSEMBLY__
#include "xen/xen.h"

/*
 * XTF puts this in ".data.page_aligned", with "aw" attributes. Given that
 * it's actually *code*, we put it in ".text" where it should be.
 */
.text
.p2align PAGE_SHIFT

.globl HYPERCALL_PAGE
HYPERCALL_PAGE:
    /* Poisoned with `ret` for safety before hypercalls are set up. */
    .fill PAGE_SIZE, 1, 0xc3
    .type HYPERCALL_PAGE, STT_OBJECT
    .size HYPERCALL_PAGE, PAGE_SIZE

/*
 * Private ABI to allow direct access to the hypercall page by
 * Mirage/Xen. May change or go away without warning.
 */
    .globl solo5__xen_HYPERCALL_PAGE
    .set solo5__xen_HYPERCALL_PAGE, HYPERCALL_PAGE

/*
 * Identify a specific hypercall in the hypercall page
 * @param name Hypercall name.
 */
#define DECLARE_HYPERCALL(name)                                             \
    .globl HYPERCALL_ ## name;                                              \
    .set   HYPERCALL_ ## name, HYPERCALL_PAGE + __HYPERVISOR_ ## name * 32; \
    .type  HYPERCALL_ ## name, STT_NOTYPE;                                  \
    .type  HYPERCALL_ ## name, STT_FUNC;                                    \
    .size  HYPERCALL_ ## name, 32

DECLARE_HYPERCALL(set_trap_table)
DECLARE_HYPERCALL(mmu_update)
DECLARE_HYPERCALL(set_gdt)
DECLARE_HYPERCALL(stack_switch)
DECLARE_HYPERCALL(set_callbacks)
DECLARE_HYPERCALL(fpu_taskswitch)
DECLARE_HYPERCALL(sched_op_compat)
DECLARE_HYPERCALL(platform_op)
DECLARE_HYPERCALL(set_debugreg)
DECLARE_HYPERCALL(get_debugreg)
DECLARE_HYPERCALL(update_descriptor)
DECLARE_HYPERCALL(memory_op)
DECLARE_HYPERCALL(multicall)
DECLARE_HYPERCALL(update_va_mapping)
DECLARE_HYPERCALL(set_timer_op)
DECLARE_HYPERCALL(event_channel_op_compat)
DECLARE_HYPERCALL(xen_version)
DECLARE_HYPERCALL(console_io)
DECLARE_HYPERCALL(physdev_op_compat)
DECLARE_HYPERCALL(grant_table_op)
DECLARE_HYPERCALL(vm_assist)
DECLARE_HYPERCALL(update_va_mapping_otherdomain)
DECLARE_HYPERCALL(iret)
DECLARE_HYPERCALL(vcpu_op)
DECLARE_HYPERCALL(set_segment_base)
DECLARE_HYPERCALL(mmuext_op)
DECLARE_HYPERCALL(xsm_op)
DECLARE_HYPERCALL(nmi_op)
DECLARE_HYPERCALL(sched_op)
DECLARE_HYPERCALL(callback_op)
DECLARE_HYPERCALL(xenoprof_op)
DECLARE_HYPERCALL(event_channel_op)
DECLARE_HYPERCALL(physdev_op)
DECLARE_HYPERCALL(hvm_op)
DECLARE_HYPERCALL(sysctl)
DECLARE_HYPERCALL(domctl)
DECLARE_HYPERCALL(kexec_op)
DECLARE_HYPERCALL(tmem_op)
/* DECLARE_HYPERCALL(xc_reserved_op) */
DECLARE_HYPERCALL(xenpmu_op)

DECLARE_HYPERCALL(arch_0)
DECLARE_HYPERCALL(arch_1)
DECLARE_HYPERCALL(arch_2)
DECLARE_HYPERCALL(arch_3)
DECLARE_HYPERCALL(arch_4)
DECLARE_HYPERCALL(arch_5)
DECLARE_HYPERCALL(arch_6)
DECLARE_HYPERCALL(arch_7)
