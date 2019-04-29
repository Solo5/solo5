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
 * hvt_dumpcore_kvm_x86_64.c: Glue between the dumpcore module and KVM.
 */

#include <sys/ioctl.h>
#include <linux/kvm.h>
#include <sys/procfs.h>
#include <sys/user.h>

#include "hvt.h"
#include "hvt_kvm.h"
#include "hvt_cpu_x86_64.h"

size_t hvt_dumpcore_prstatus_size(void)
{
    return sizeof (prstatus_t);
}

int hvt_dumpcore_write_prstatus(int fd, struct hvt *hvt, void *cookie)
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

    if (ioctl(hvt->b->vcpufd, KVM_GET_SREGS, &sregs) == -1) {
        warn("dumpcore: KVM: ioctl(KVM_GET_SREGS) failed");
        return -1;
    }

    if (ioctl(hvt->b->vcpufd, KVM_GET_REGS, &kregs) == -1) {
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
        assert(sizeof(struct x86_trap_regs) < HVT_HALT_COOKIE_MAX);
        struct x86_trap_regs *regs = (struct x86_trap_regs *)cookie;
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

int hvt_dumpcore_supported()
{
    return 0;
}
