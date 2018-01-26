/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of ukvm, a unikernel monitor.
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
 * ukvm_gdb_freebsd_x86_64.c: glue between the GDB server (at ukvm_modules_gdb.c)
 * and FreeBSD's vmm.
 */

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <machine/vmm.h>
#include <sys/param.h>
#include <sys/cpuset.h>
#include <machine/vmm_dev.h>

#include "ukvm.h"
#include "ukvm_hv_freebsd.h"
#include "ukvm_gdb_x86_64.h"
#include "ukvm_gdb.h"

#if 0
#define KVM_GUESTDBG_ENABLE		0x00000001
#define KVM_GUESTDBG_SINGLESTEP		0x00000002

static int ukvm_gdb_update_guest_debug(struct ukvm_hv *hv)
{
    struct kvm_guest_debug dbg = {0};
    struct breakpoint_t *bp;
    const uint8_t type_code[] = {
        /* Break on instruction execution only. */
        [GDB_BREAKPOINT_HW] = 0x0,
        /* Break on data writes only. */
        [GDB_WATCHPOINT_WRITE] = 0x1,
        /* Break on data reads only. */
        [GDB_WATCHPOINT_READ] = 0x2,
        /* Break on data reads or writes but not instruction fetches. */
        [GDB_WATCHPOINT_ACCESS] = 0x3
    };
    const uint8_t len_code[] = {
        /*
         * 00 \xe2\x80\x94 1-byte length.
         * 01 \xe2\x80\x94 2-byte length.
         * 10 \xe2\x80\x94 8-byte length.
         * 11 \xe2\x80\x94 4-byte length.
         */
        [1] = 0x0, [2] = 0x1, [4] = 0x3, [8] = 0x2
    };
    int n = 0;

    if (stepping)
        dbg.control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_SINGLESTEP;

    if (!SLIST_EMPTY(&sw_breakpoints))
        dbg.control |= KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_USE_SW_BP;

    if (!SLIST_EMPTY(&hw_breakpoints)) {
        dbg.control |= KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_USE_HW_BP;

        /* Enable global breakpointing (across all threads) on the control
         * debug register. */
        dbg.arch.debugreg[7] = 1 << 9;
        dbg.arch.debugreg[7] |= 1 << 10;
        SLIST_FOREACH(bp, &hw_breakpoints, entries) {
            assert(bp->type != GDB_BREAKPOINT_SW);
            dbg.arch.debugreg[n] = bp->addr;
            /* global breakpointing */
            dbg.arch.debugreg[7] |= (2 << (n * 2));
            /* read/write fields */
            dbg.arch.debugreg[7] |= (type_code[bp->type] << (16 + n*4));
            /* Length fields */
            dbg.arch.debugreg[7] |= ((uint32_t)len_code[bp->len] << (18 + n*4));
            n++;
        }
    }

    if (ioctl(hv->b->vcpufd, KVM_SET_GUEST_DEBUG, &dbg) == -1) {
        /* The KVM_CAP_SET_GUEST_DEBUG capbility is not available. */
        err(1, "KVM_SET_GUEST_DEBUG failed");
        return -1;
    }

    return 0;
}
#endif

int ukvm_gdb_supported(void)
{
    return -1;
}

int ukvm_gdb_read_registers(struct ukvm_hv *hv,
                            uint8_t *registers,
                            size_t *len)
{
    return -1;
}

int ukvm_gdb_write_registers(struct ukvm_hv *hv,
                             uint8_t *registers,
                             size_t len)
{
    return -1;
}

int ukvm_gdb_enable_ss(struct ukvm_hv *hv)
{
VM_REG_GUEST_DR7
    return -1;
}

int ukvm_gdb_disable_ss(struct ukvm_hv *hv)
{
    return -1;
}

int ukvm_gdb_read_last_signal(struct ukvm_hv *hv, int *signal)
{
    return -1;
}

int ukvm_gdb_add_breakpoint(struct ukvm_hv *hv, uint32_t type,
                            ukvm_gpa_t addr, size_t len)
{
    return -1;
}

int ukvm_gdb_remove_breakpoint(struct ukvm_hv *hv, uint32_t type,
                               ukvm_gpa_t addr, size_t len)
{
    return -1;
}
