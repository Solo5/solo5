/*
 * Copyright (c) 2015-2019 Contributors as noted in the AUTHORS file
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
 * hvt_gdb_update_guest_debug is based on update_guest_debug from the QEMU
 * source code, target/i386/kvm.c, which is:
 *
 * Copyright (C) 2006-2008 Qumranet Technologies
 * Copyright IBM, Corp. 2008
 */

/*
 * hvt_gdb_kvm_x86_64.c: glue between the GDB server (at hvt_modules_gdb.c)
 * and KVM.
 */

#include <stdint.h>
#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <linux/kvm.h>
#include <linux/kvm_para.h>

#include "hvt.h"
#include "hvt_kvm.h"
#include "hvt_gdb_x86_64.h"
#include "hvt_gdb.h"
#include "queue.h"

struct breakpoint_t {
    gdb_breakpoint_type type;
    hvt_gpa_t addr;
    size_t len;
    uint32_t refcount;
    uint8_t saved_insn; /* for software breakpoints */

    SLIST_ENTRY(breakpoint_t) entries;
};

SLIST_HEAD(breakpoints_head, breakpoint_t);
static struct breakpoints_head sw_breakpoints;
static struct breakpoints_head hw_breakpoints;

/* The Intel SDM specifies that the DR7 has space for 4 breakpoints. */
#define MAX_HW_BREAKPOINTS             4
static uint32_t nr_hw_breakpoints = 0;

/* Stepping is disabled by default. */
static bool stepping = false;
/* This is the trap instruction used for software breakpoints. */
static const uint8_t int3 = 0xcc;

static uint16_t to_fsave_tag(uint16_t fsw, uint8_t tag, const struct fpu_reg regs[FPU_REGS]);

static int kvm_arch_insert_sw_breakpoint(struct hvt *hvt, struct breakpoint_t *bp)
{
    /* The address check at the GDB server just returned an error if addr was
     * bad. HVT_CHECKED_GPA_P will panic if that's the case. */
    uint8_t *insn = HVT_CHECKED_GPA_P(hvt, bp->addr, bp->len);
    bp->saved_insn = *insn;
    /*
     * We just modify the first byte even if the instruction is multi-byte.
     * The debugger keeps track of the length of the instruction. The
     * consequence of this is that we don't have to set all other bytes as
     * NOP's.
     */
    *insn = int3;
    return 0;
}

static int kvm_arch_remove_sw_breakpoint(struct hvt *hvt, struct breakpoint_t *bp)
{
    uint8_t *insn = HVT_CHECKED_GPA_P(hvt, bp->addr, bp->len);
    assert(*insn == int3);
    *insn = bp->saved_insn;
    return 0;
}

static int hvt_gdb_update_guest_debug(struct hvt *hvt)
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
         * 00 — 1-byte length.
         * 01 — 2-byte length.
         * 10 — 8-byte length.
         * 11 — 4-byte length.
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

    if (ioctl(hvt->b->vcpufd, KVM_SET_GUEST_DEBUG, &dbg) == -1) {
        /* The KVM_CAP_SET_GUEST_DEBUG capbility is not available. */
        err(1, "KVM_SET_GUEST_DEBUG failed");
        return -1;
    }

    return 0;
}

static struct breakpoint_t *bp_list_find(gdb_breakpoint_type type, hvt_gpa_t addr, size_t len)
{
    struct breakpoint_t *bp;

    switch (type) {
    case GDB_BREAKPOINT_SW:
        SLIST_FOREACH(bp, &sw_breakpoints, entries) {
            if (bp->addr == addr && bp->len == len)
                return bp;
        }
        break;

    case GDB_BREAKPOINT_HW:
    case GDB_WATCHPOINT_WRITE:
    case GDB_WATCHPOINT_READ:
    case GDB_WATCHPOINT_ACCESS:
        /* We only support hardware watchpoints. */
        SLIST_FOREACH(bp, &hw_breakpoints, entries) {
            if (bp->addr == addr && bp->len == len)
                return bp;
        }
        break;

    default:
        assert(0);
    }

    return NULL;
}

/*
 * Adds a new breakpoint to the list of breakpoints.  Returns the found or
 * created breakpoint. Returns NULL in case of failure or if we reached the max
 * number of allowed hardware breakpoints (4).
 */
static struct breakpoint_t *bp_list_insert(gdb_breakpoint_type type,
                                           hvt_gpa_t addr,
                                           size_t len)
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

    switch (type) {
    case GDB_BREAKPOINT_SW:
        SLIST_INSERT_HEAD(&sw_breakpoints, bp, entries);
        break;

    case GDB_BREAKPOINT_HW:
    case GDB_WATCHPOINT_WRITE:
    case GDB_WATCHPOINT_READ:
    case GDB_WATCHPOINT_ACCESS:
        /* We only support hardware watchpoints. */
        if (nr_hw_breakpoints == MAX_HW_BREAKPOINTS)
            return NULL;
        nr_hw_breakpoints++;
        SLIST_INSERT_HEAD(&hw_breakpoints, bp, entries);
        break;

    default:
        assert(0);
    }

    return bp;
}

/*
 * Removes a breakpoint from the list of breakpoints.
 * Returns -1 if the breakpoint is not in the list.
 */
static int bp_list_remove(gdb_breakpoint_type type,
                          hvt_gpa_t addr, size_t len)
{
    struct breakpoint_t *bp = NULL;

    bp = bp_list_find(type, addr, len);
    if (!bp)
        return -1;

    bp->refcount--;
    if (bp->refcount > 0)
        return 0;

    switch (type) {
    case GDB_BREAKPOINT_SW:
        SLIST_REMOVE(&sw_breakpoints, bp, breakpoint_t, entries);
        break;

    case GDB_BREAKPOINT_HW:
    case GDB_WATCHPOINT_WRITE:
    case GDB_WATCHPOINT_READ:
    case GDB_WATCHPOINT_ACCESS:
        /* We only support hardware watchpoints. */
        SLIST_REMOVE(&hw_breakpoints, bp, breakpoint_t, entries);
        nr_hw_breakpoints--;
        break;

    default:
        assert(0);
    }

    free(bp);

    return 0;
}

int hvt_gdb_supported(void)
{
    return 0;
}

int hvt_gdb_read_registers(struct hvt *hvt,
                            uint8_t *registers,
                            size_t *len)
{
    struct kvm_regs kregs;
    struct kvm_sregs sregs;
    struct kvm_fpu fregs;
    struct hvt_gdb_regs *gregs = (struct hvt_gdb_regs *) registers;
    int ret;

    if (*len < sizeof(struct hvt_gdb_regs))
        return -1;
    *len = sizeof(struct hvt_gdb_regs);

    ret = ioctl(hvt->b->vcpufd, KVM_GET_REGS, &kregs);
    if (ret == -1) {
        err(1, "KVM_GET_REGS");
        return -1;
    }

    ret = ioctl(hvt->b->vcpufd, KVM_GET_SREGS, &sregs);
    if (ret == -1) {
        err(1, "KVM_GET_SREGS");
        return -1;
    }

    ret = ioctl(hvt->b->vcpufd, KVM_GET_FPU, &fregs);
    if (ret == -1) {
        err(1, "KVM_GET_FPU");
        return -1;
    }

    // There's a bunch of reserved areas in the FPU section.
    // It's easier to just zero out everything.
    memset(gregs, 0, sizeof(*gregs));

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

    // NOTE:
    // The following code is tricky. KVM provides FPU state as dumped by FXSAVE
    // x86 instruction, which has been available since Pentium 2/AMD K6.
    // GDB on the other hand has wire format hardcoded as given by FSAVE
    // instruction, probably for some very stupid reason. Therefore, a conversion
    // is required.
    // This doesn't result in an exact FPU state that could be i.e. loaded back
    // with KVM_SET_FPU, yet this still can be handful for debugging.

    // Copy STx/MMx registers.
    for(int i = 0; i < FPU_REGS; i++) {
        memcpy(&gregs->st[i].data, fregs.fpr[i], sizeof(gregs->st[i].data));
    }
    // Bottom 16 bits of these fields are reserved?
    gregs->fctrl = (uint32_t)fregs.fcw << 16;
    gregs->fstat = (uint32_t)fregs.fsw << 16;

    gregs->ftag = to_fsave_tag(fregs.fsw, fregs.ftwx, gregs->st);
    gregs->fop = fregs.last_opcode;

    // These fields are 8 and 24 bits for segment and offset respectively in x86,
    // but in GDB wire protocol they all are 32 bits. Hence, sending as much info
    // as we can here.
    gregs->fiseg = (fregs.last_ip >> 32) & 0xffffffff;
    gregs->fioff = fregs.last_ip & 0xffffffff;
    gregs->foseg = (fregs.last_dp >> 32) & 0xffffffff;
    gregs->fooff = fregs.last_dp & 0xffffffff;

    return 0;
}

int hvt_gdb_write_registers(struct hvt *hvt,
                             uint8_t *registers,
                             size_t len)
{
    struct kvm_regs kregs;
    struct kvm_sregs sregs;
    struct hvt_gdb_regs *gregs = (struct hvt_gdb_regs *) registers;
    int ret;

    /* Let's read all registers just in case we miss filling one of them. */
    ret = ioctl(hvt->b->vcpufd, KVM_GET_REGS, &kregs);
    if (ret == -1) {
        err(1, "KVM_GET_REGS");
        return -1;
    }

    ret = ioctl(hvt->b->vcpufd, KVM_GET_SREGS, &sregs);
    if (ret == -1) {
        err(1, "KVM_GET_REGS");
        return -1;
    }

    if (len < sizeof(struct hvt_gdb_regs))
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

    ret = ioctl(hvt->b->vcpufd, KVM_SET_REGS, &kregs);
    if (ret == -1) {
        err(1, "KVM_SET_REGS");
        return -1;
    }

    ret = ioctl(hvt->b->vcpufd, KVM_SET_SREGS, &sregs);
    if (ret == -1) {
        err(1, "KVM_SET_SREGS");
        return -1;
    }

    return 0;
}

int hvt_gdb_add_breakpoint(struct hvt *hvt,
                            gdb_breakpoint_type type,
                            hvt_gpa_t addr, size_t len)
{
    struct breakpoint_t *bp;

    assert(type < GDB_BREAKPOINT_MAX);

    if (bp_list_find(type, addr, len))
        return 0;

    bp = bp_list_insert(type, addr, len);
    if (bp == NULL)
        return -1;

    if (type == GDB_BREAKPOINT_SW)
        kvm_arch_insert_sw_breakpoint(hvt, bp);

    if (hvt_gdb_update_guest_debug(hvt) == -1)
        return -1;

    return 0;
}

int hvt_gdb_remove_breakpoint(struct hvt *hvt,
                               gdb_breakpoint_type type,
                               hvt_gpa_t addr, size_t len)
{
    struct breakpoint_t *bp;

    assert(type < GDB_BREAKPOINT_MAX);

    if (type == GDB_BREAKPOINT_SW) {
        bp = bp_list_find(type, addr, len);
        if (bp)
            kvm_arch_remove_sw_breakpoint(hvt, bp);
    }

    if (bp_list_remove(type, addr, len) == -1)
        return -1;

    if (hvt_gdb_update_guest_debug(hvt) == -1)
        return -1;

    return 0;
}

int hvt_gdb_enable_ss(struct hvt *hvt)
{
    stepping = true;

    if (hvt_gdb_update_guest_debug(hvt) == -1)
        return -1;

    return 0;
}

int hvt_gdb_disable_ss(struct hvt *hvt)
{
    stepping = false;

    if (hvt_gdb_update_guest_debug(hvt) == -1)
        return -1;

    return 0;
}

/*
 * Reads the current KVM exit code and maps it to a GDB signal value.
 * GDB signals are regular UNIX signals, so the job here is to map
 * exits to signals (which does not always make sense).
 */
int hvt_gdb_read_last_signal(struct hvt *hvt, int *signal)
{
    switch (hvt->b->vcpurun->exit_reason) {
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

// Convert FXSAVE tag to full tag format from FSAVE.
// This is what GDB expects in its wire protocol for some dumb reason.
static uint16_t to_fsave_tag(uint16_t fsw, uint8_t tag, const struct fpu_reg regs[FPU_REGS])
{
  int stack_top = (fsw >> 11) & 0x7;

  uint16_t fsave_tag = 0;
  for (int phys_idx = 0; phys_idx < FPU_REGS; phys_idx++) {
    bool fxsave_bit = (tag & (1 << phys_idx)) != 0;
    uint8_t fsave_bits = 0;

    if (fxsave_bit) {
      int st_idx = (phys_idx + FPU_REGS - stack_top) % FPU_REGS;
      const unsigned char *st = regs[st_idx].data;
      bool integer_bit = (st[7] & 0x80) != 0;
      uint16_t exponent = ((st[9] & 0x7f) << 8) | st[8];
      uint64_t fraction = (((uint64_t)st[7] & 0x7f) << 56)
                        | ((uint64_t)st[6] << 48)
                        | ((uint64_t)st[5] << 40)
                        | ((uint64_t)st[4] << 32)
                        | ((uint64_t)st[3] << 24)
                        | ((uint64_t)st[2] << 16)
                        | ((uint64_t)st[1] << 8)
                        |  (uint64_t)st[0];

      if (exponent == 0x7fff) {
        // Infinity, NaN, pseudo-infinity, or pseudo-NaN.
        // Mostly the case for MMX values.
        fsave_bits = 2;
      } else if (exponent == 0) {
        // J bit.
        fsave_bits = !integer_bit && !fraction ? 1 : 2;
      } else if (integer_bit) {
        fsave_bits = 0;
      } else {
        fsave_bits = 1;
      }
    } else {
      fsave_bits = 3;
    }
    fsave_tag |= (fsave_bits << (phys_idx * 2));
  }

  return fsave_tag;
}
