/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of ukvm, a unikernel monitor.
 * 
 * ukvm_gdb_update_guest_debug is based on update_guest_debug from the QEMU
 * source code, target/i386/kvm.c, which is:
 *
 * Copyright (C) 2006-2008 Qumranet Technologies
 * Copyright IBM, Corp. 2008
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
 * ukvm_gdb_kvm_x86_64.c: glue between the GDB server (at ukvm_modules_gdb.c)
 * and KVM.
 * XXX: this file is linked in even if we don't use the GDB module.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>

#include <linux/kvm.h>
#include <linux/kvm_para.h>

#include "ukvm.h"
#include "ukvm_hv_kvm.h"
#include "ukvm_gdb_x86_64.h"
#include "ukvm_gdb.h"
#include "queue.h"

struct breakpoint_t {
    uint32_t type;
    uint64_t addr;
    uint32_t len;
    uint32_t refcount;
    uint8_t saved_insn; /* for software breakpoints */

    SLIST_ENTRY(breakpoint_t) entries;
};

SLIST_HEAD(breakpoints_head, breakpoint_t);
static struct breakpoints_head sw_breakpoints;
static struct breakpoints_head hw_breakpoints;

/* Not really documented. But, QEMU uses a hardcoded max of 4. */
#define MAX_HW_BREAKPOINTS             4
static uint32_t nr_hw_breakpoints = 0;

/* Stepping is disabled by default. */
static uint32_t stepping = 0;
/* This is the trap instruction used for software breakpoints. */
static const uint8_t int3 = 0xcc;

static int kvm_arch_insert_sw_breakpoint(struct ukvm_hv *hv, struct breakpoint_t *bp)
{
    bp->saved_insn = *((uint8_t *)hv->mem + bp->addr);
    /* It was already checked that addr is good. */
    *((uint8_t *)hv->mem + bp->addr) = int3;
    return 0;
}

static int kvm_arch_remove_sw_breakpoint(struct ukvm_hv *hv, struct breakpoint_t *bp)
{
    assert((uint8_t) *((uint8_t *)hv->mem + bp->addr) == int3);
    /* It was already checked that addr is good. */
    *((uint8_t *)hv->mem + bp->addr) = bp->saved_insn;
    return 0;
}

static int ukvm_gdb_update_guest_debug(struct ukvm_hv *hv)
{
    struct kvm_guest_debug dbg = {0};
    struct breakpoint_t *bp;

    const uint8_t type_code[] = {
        [GDB_BREAKPOINT_HW] = 0x0,
        [GDB_WATCHPOINT_WRITE] = 0x1,
        [GDB_WATCHPOINT_ACCESS] = 0x3
    };
    const uint8_t len_code[] = {
        [1] = 0x0, [2] = 0x1, [4] = 0x3, [8] = 0x2
    };
    int n = 0;

    if (stepping)
        dbg.control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_SINGLESTEP;

    if (!SLIST_EMPTY(&sw_breakpoints)) {
        dbg.control |= KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_USE_SW_BP;
    } else {
        dbg.control |= KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_USE_HW_BP;
        /* Not documented magic numbers from QEMU */
        dbg.arch.debugreg[7] = 0x0600;
        SLIST_FOREACH(bp, &hw_breakpoints, entries) {
            dbg.arch.debugreg[n] = bp->addr;
            dbg.arch.debugreg[7] |= (2 << (n * 2)) |
                (type_code[bp->type] << (16 + n*4)) |
                ((uint32_t)len_code[bp->len] << (18 + n*4));
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

static struct breakpoint_t *bp_list_find(uint32_t type, uint64_t addr, uint32_t len)
{
    struct breakpoint_t *bp;

    if (type == GDB_BREAKPOINT_SW) {
        SLIST_FOREACH(bp, &sw_breakpoints, entries) {
            if (bp->addr == addr && bp->len == len)
                return bp;
        }
    } else {
        SLIST_FOREACH(bp, &hw_breakpoints, entries) {
            if (bp->addr == addr && bp->len == len)
                return bp;
        }
    }

    return NULL;
}

/*
 * Adds a new breakpoint to the list of breakpoints 
 * Returns the found or created breakpoint, NULL if otherwise.
 */
static struct breakpoint_t *bp_list_insert(uint32_t type, uint64_t addr,
                                           uint32_t len)
{
    struct breakpoint_t *bp;

    bp = bp_list_find(type, addr, len);
    if (bp) {
        bp->refcount++;
        return bp;
    }

    bp = malloc(sizeof (struct breakpoint_t));
    if (bp == NULL)
        return NULL;

    bp->addr = addr;
    bp->type = type;
    bp->len = len;
    bp->refcount = 1;

    if (type == GDB_BREAKPOINT_SW) {
        SLIST_INSERT_HEAD(&sw_breakpoints, bp, entries);
    } else {
        if (nr_hw_breakpoints == MAX_HW_BREAKPOINTS)
            return NULL;
        nr_hw_breakpoints++;
        SLIST_INSERT_HEAD(&hw_breakpoints, bp, entries);
    }

    return bp;
}

/*
 * Removes a breakpoint from the list of breakpoints.
 * Returns -1 if the breakpoint is not in the list.
 */
static int bp_list_remove(uint32_t type, uint64_t addr, uint32_t len)
{
    struct breakpoint_t *bp = NULL;

    bp = bp_list_find(type, addr, len);
    if (!bp)
        return -1;

    bp->refcount--;
    if (bp->refcount > 0)
        return 0;

    if (type == GDB_BREAKPOINT_SW) {
        SLIST_REMOVE(&sw_breakpoints, bp, breakpoint_t, entries);
    } else {
        SLIST_REMOVE(&hw_breakpoints, bp, breakpoint_t, entries);
        nr_hw_breakpoints--;
    }

    free(bp);

    return 0;
}

/*
 * Fills *registers with a stream of hexadecimal digits for each register in
 * GDB register order, where each register is in target endian order.
 */
int ukvm_gdb_read_registers(struct ukvm_hv *hv,
                            uint8_t *registers,
                            uint64_t *len)
{
    struct kvm_regs kregs;
    struct kvm_sregs sregs;
    struct ukvm_gdb_regs *gregs = (struct ukvm_gdb_regs *) registers;
    int ret;

    ret = ioctl(hv->b->vcpufd, KVM_GET_REGS, &kregs);
    if (ret == -1) {
        err(1, "KVM_GET_REGS");
        return -1;
    }

    ret = ioctl(hv->b->vcpufd, KVM_GET_SREGS, &sregs);
    if (ret == -1) {
        err(1, "KVM_GET_REGS");
        return -1;
    }

    if (*len < sizeof(struct ukvm_gdb_regs))
        return -1;

    *len = sizeof(struct ukvm_gdb_regs);

    gregs->rax = kregs.rax;
    gregs->rbx = kregs.rbx;
    gregs->rcx = kregs.rcx;
    gregs->rdx = kregs.rdx;

    gregs->rsi = kregs.rsi;
    gregs->rdi = kregs.rdi;
    gregs->rbp = kregs.rbp;
    gregs->rsp = kregs.rsp;

    gregs->r8 = kregs.r8;
    gregs->r9 = kregs.r9;
    gregs->r10 = kregs.r10;
    gregs->r11 = kregs.r11;

    gregs->rip = kregs.rip;
    gregs->eflags = kregs.rflags;

    gregs->cs = sregs.cs.selector;
    gregs->ss = sregs.ss.selector;
    gregs->ds = sregs.ds.selector;
    gregs->es = sregs.es.selector;
    gregs->fs = sregs.fs.selector;
    gregs->gs = sregs.gs.selector;

    return 0;
}

/*
 * Writes the shadow registers from a stream of hexadecimal digits for each register in
 * GDB register order, where each register is in target endian order.
 */
int ukvm_gdb_write_registers(struct ukvm_hv *hv,
                             uint8_t *registers,
                             uint64_t len)
{
    struct kvm_regs kregs;
    struct kvm_sregs sregs;
    struct ukvm_gdb_regs *gregs = (struct ukvm_gdb_regs *) registers;
    int ret;

    /* Let's read all registers just in case we miss filling one of them. */
    ret = ioctl(hv->b->vcpufd, KVM_GET_REGS, &kregs);
    if (ret == -1) {
        err(1, "KVM_GET_REGS");
        return -1;
    }

    ret = ioctl(hv->b->vcpufd, KVM_GET_SREGS, &sregs);
    if (ret == -1) {
        err(1, "KVM_GET_REGS");
        return -1;
    }

    if (len < sizeof(struct ukvm_gdb_regs))
        return -1;

    kregs.rax = gregs->rax;
    kregs.rbx = gregs->rbx;
    kregs.rcx = gregs->rcx;
    kregs.rdx = gregs->rdx;

    kregs.rsi = gregs->rsi;
    kregs.rdi = gregs->rdi;
    kregs.rbp = gregs->rbp;
    kregs.rsp = gregs->rsp;

    kregs.r8 = gregs->r8;
    kregs.r9 = gregs->r9;
    kregs.r10 = gregs->r10;
    kregs.r11 = gregs->r11;

    kregs.rip = gregs->rip;
    kregs.rflags = gregs->eflags;

    /* XXX: not sure if just setting .selector is enough. */
    sregs.cs.selector = gregs->cs;
    sregs.ss.selector = gregs->ss;
    sregs.ds.selector = gregs->ds;
    sregs.es.selector = gregs->es;
    sregs.fs.selector = gregs->fs;
    sregs.gs.selector = gregs->gs;

    ret = ioctl(hv->b->vcpufd, KVM_SET_REGS, &kregs);
    if (ret == -1) {
        err(1, "KVM_GET_REGS");
        return -1;
    }

    ret = ioctl(hv->b->vcpufd, KVM_SET_SREGS, &sregs);
    if (ret == -1) {
        err(1, "KVM_GET_REGS");
        return -1;
    }

    return 0;
}

int ukvm_gdb_add_breakpoint(struct ukvm_hv *hv, uint32_t type,
                            uint64_t addr, uint32_t len)
{
    struct breakpoint_t *bp;

    if (bp_list_find(type, addr, len))
        return 0;

    bp = bp_list_insert(type, addr, len);
    if (bp == NULL)
        return -1;

    if (type == GDB_BREAKPOINT_SW)
        kvm_arch_insert_sw_breakpoint(hv, bp);

    if (ukvm_gdb_update_guest_debug(hv) < 0)
        return -1;

    return 0;
}

int ukvm_gdb_remove_breakpoint(struct ukvm_hv *hv, uint32_t type,
                               uint64_t addr, uint32_t len)
{
    struct breakpoint_t *bp;

    if (type == GDB_BREAKPOINT_SW) {
        bp = bp_list_find(type, addr, len);
        if (bp)
            kvm_arch_remove_sw_breakpoint(hv, bp);
    }

    if (bp_list_remove(type, addr, len) < 0)
        return -1;

    if (ukvm_gdb_update_guest_debug(hv) < 0)
        return -1;

    return 0;
}

int ukvm_gdb_enable_ss(struct ukvm_hv *hv)
{
    stepping = 1;

    if (ukvm_gdb_update_guest_debug(hv) < 0)
        return -1;

    return 0;
}

int ukvm_gdb_disable_ss(struct ukvm_hv *hv)
{
    stepping = 0;

    if (ukvm_gdb_update_guest_debug(hv) < 0)
        return -1;

    return 0;
}

/*
 * Reads the current KVM exit code and maps it to a GDB signal value.
 * GDB signals are regular UNIX signals, so the job here is to map
 * exits to signals (which does not always make sense).
 */
int ukvm_gdb_read_last_signal(struct ukvm_hv *hv, int *signal)
{
    switch (hv->b->vcpurun->exit_reason) {
    case KVM_EXIT_DEBUG:
        *signal = GDB_SIGNAL_TRAP;
        break;

    case KVM_EXIT_HLT:
        *signal = GDB_SIGNAL_TERM;
        break;

    case KVM_EXIT_IO:
        *signal = GDB_SIGNAL_IO;
        break;

    case KVM_EXIT_FAIL_ENTRY:
    case KVM_EXIT_INTERNAL_ERROR:
        *signal = GDB_SIGNAL_SEGV;
        break;

    default:
        *signal = GDB_SIGNAL_DEFAULT;
    }

    return 0;
}
