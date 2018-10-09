/*
 * Copyright (c) 2015-2018 Contributors as noted in the AUTHORS file
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
 * hvt_dumpcore_openbsd_x86_64.c: Glue between the dumpcore module and OpenBSD
 * (x86_64).
 *
 * Please note the core generated on OpenBSD is not readable by gdb on OpenBSD.
 */

#include <sys/ioctl.h>
#include <machine/vmmvar.h>

#include "hvt.h"
#include "hvt_openbsd.h"
#include "hvt_cpu_x86_64.h"

int hvt_dumpcore_supported()
{
    return sizeof (prstatus_t);
}

size_t hvt_dumpcore_prstatus_size(void)
{
    return sizeof(prstatus_t);
}

int hvt_dumpcore_write_prstatus(int fd, struct hvt *hvt, void *cookie)
{
    struct hvt_b      *hvb = hvt->b;
    struct vcpu_reg_state vrs = *hvb->vrs;

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

    uregs->r8 = vrs.vrs_gprs[VCPU_REGS_R8];
    uregs->r9 = vrs.vrs_gprs[VCPU_REGS_R9];
    uregs->r10 = vrs.vrs_gprs[VCPU_REGS_R10];
    uregs->r11 = vrs.vrs_gprs[VCPU_REGS_R11];
    uregs->r12 = vrs.vrs_gprs[VCPU_REGS_R12];
    uregs->r13 = vrs.vrs_gprs[VCPU_REGS_R13];
    uregs->r14 = vrs.vrs_gprs[VCPU_REGS_R14];
    uregs->r15 = vrs.vrs_gprs[VCPU_REGS_R15];
    uregs->rbp = vrs.vrs_gprs[VCPU_REGS_RBP];
    uregs->rsp = vrs.vrs_gprs[VCPU_REGS_RSP];
    uregs->rdi = vrs.vrs_gprs[VCPU_REGS_RDI];
    uregs->rsi = vrs.vrs_gprs[VCPU_REGS_RSI];
    uregs->rdx = vrs.vrs_gprs[VCPU_REGS_RDX];
    uregs->rcx = vrs.vrs_gprs[VCPU_REGS_RCX];
    uregs->rbx = vrs.vrs_gprs[VCPU_REGS_RBX];
    uregs->rax = vrs.vrs_gprs[VCPU_REGS_RAX];
    uregs->rip = vrs.vrs_gprs[VCPU_REGS_RIP];
    uregs->eflags = vrs.vrs_gprs[VCPU_REGS_RFLAGS];

    uregs->cs = vrs.vrs_sregs[VCPU_REGS_CS].vsi_sel;
    uregs->ss = vrs.vrs_sregs[VCPU_REGS_SS].vsi_sel;
    uregs->ds = vrs.vrs_sregs[VCPU_REGS_DS].vsi_sel;
    uregs->es = vrs.vrs_sregs[VCPU_REGS_ES].vsi_sel;
    uregs->fs = vrs.vrs_sregs[VCPU_REGS_FS].vsi_sel;
    uregs->gs = vrs.vrs_sregs[VCPU_REGS_GS].vsi_sel;
    uregs->fs_base = vrs.vrs_sregs[VCPU_REGS_FS].vsi_base;
    uregs->gs_base = vrs.vrs_sregs[VCPU_REGS_GS].vsi_base;

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
