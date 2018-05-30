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
 * ukvm_dumpcore_kvm_x86_64.c: Glue between the dumpcore module and KVM.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <elf.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/kvm.h>

#include <sys/procfs.h>
#include <sys/user.h>

#include "ukvm.h"
#include "ukvm_hv_kvm.h"
#include "ukvm_cpu_x86_64.h"

struct cpu_trap_regs {
    uint64_t cr2;
    uint64_t ec;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

size_t ukvm_dumpcore_prstatus_size(void)
{
    return sizeof (prstatus_t);
}

int ukvm_dumpcore_write_prstatus(int fd, struct ukvm_hv *hv, void *cookie)
{
    prstatus_t prstatus;
    memset(&prstatus, 0, sizeof prstatus);
    /*
     * prstatus_t.pr_reg is actually a (struct user_regs_struct) in disguise.
     * This is all non-portable, but both GLIBC and musl expose the struct
     * definition in <sys/user.h>, so just use the sane interface instead of
     * dealing with SVR4 garbage.
     */
    struct user_regs_struct *uregs =
        (struct user_regs_struct *)&prstatus.pr_reg;
    struct kvm_regs kregs;
    struct kvm_sregs sregs;

    if (ioctl(hv->b->vcpufd, KVM_GET_SREGS, &sregs) == -1) {
        warn("dumpcore: KVM: ioctl(KVM_GET_SREGS) failed");
        return -1;
    }

    if (ioctl(hv->b->vcpufd, KVM_GET_REGS, &kregs) == -1) {
        warn("dumpcore: KVM: ioctl(KVM_GET_REGS) failed");
        return -1;
    }

    uregs->r8 = kregs.r8;
    uregs->r9 = kregs.r9;
    uregs->r10 = kregs.r10;
    uregs->r11 = kregs.r11;
    uregs->r12 = kregs.r12;
    uregs->r13 = kregs.r13;
    uregs->r14 = kregs.r14;
    uregs->r15 = kregs.r15;
    uregs->rbp = kregs.rbp;
    uregs->rsp = kregs.rsp;
    uregs->rdi = kregs.rdi;
    uregs->rsi = kregs.rsi;
    uregs->rdx = kregs.rdx;
    uregs->rcx = kregs.rcx;
    uregs->rbx = kregs.rbx;
    uregs->rax = kregs.rax;
    uregs->rip = kregs.rip;
    uregs->eflags = kregs.rflags;

    uregs->cs = sregs.cs.selector;
    uregs->ss = sregs.ss.selector;
    uregs->ds = sregs.ds.selector;
    uregs->es = sregs.es.selector;
    uregs->fs = sregs.fs.selector;
    uregs->gs = sregs.gs.selector;
    uregs->fs_base = sregs.fs.base;
    uregs->gs_base = sregs.gs.base;

    /*
     * If the guest provided us with register data from a trap handler, use
     * that.
     */
    if (cookie) {
        assert(sizeof(struct cpu_trap_regs) < UKVM_COOKIE_MAX);
        struct cpu_trap_regs *regs = (struct cpu_trap_regs *)cookie;
        uregs->rip = regs->rip;
        uregs->cs = regs->cs;
        uregs->eflags = regs->rflags;
        uregs->rsp = regs->rsp;
        uregs->ss = regs->ss;
    }

    ssize_t nbytes;
    nbytes = write(fd, &prstatus, sizeof prstatus);
    if (nbytes < 0) {
        warn("dumpcore: Error writing prstatus");
        return -1;
    }
    else if (nbytes != sizeof prstatus) {
        warnx("dumpcore: Short write() writing prstatus: %zd", nbytes);
        return -1;
    }

    return 0;
}

int ukvm_is_arch_supported()
{
    return 1;
}
