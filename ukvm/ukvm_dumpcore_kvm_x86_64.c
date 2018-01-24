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
 * ukvm_dumpcore_kvm_x86_64.c: glue between the dumpcore ukvm_module
 * and x86's vmm.
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

#include "ukvm.h"
#include "ukvm_hv_kvm.h"
#include "ukvm_cpu_x86_64.h"

void ukvm_hv_fill_elf_prstatus(x86_elf_prstatus *prstatus,
	 struct ukvm_hv *hv, struct ukvm_dumpcore *info)
{
    memset(prstatus, 0, sizeof(x86_elf_prstatus));
    prstatus->regs.r8 = hv->b->kregs.r8;
    prstatus->regs.r9 = hv->b->kregs.r9;
    prstatus->regs.r10 = hv->b->kregs.r10;
    prstatus->regs.r11 = hv->b->kregs.r11;
    prstatus->regs.r12 = hv->b->kregs.r12;
    prstatus->regs.r13 = hv->b->kregs.r13;
    prstatus->regs.r14 = hv->b->kregs.r14;
    prstatus->regs.r15 = hv->b->kregs.r15;
    prstatus->regs.rbp = hv->b->kregs.rbp;
    prstatus->regs.rsp = hv->b->kregs.rsp;
    prstatus->regs.rdi = hv->b->kregs.rdi;
    prstatus->regs.rsi = hv->b->kregs.rsi;
    prstatus->regs.rdx = hv->b->kregs.rdx;
    prstatus->regs.rcx = hv->b->kregs.rcx;
    prstatus->regs.rbx = hv->b->kregs.rbx;
    prstatus->regs.rax = hv->b->kregs.rax;
    prstatus->regs.rip = hv->b->kregs.rip;
    prstatus->regs.eflags = hv->b->kregs.rflags;

    prstatus->regs.cs = hv->b->sregs.cs.selector;
    prstatus->regs.ss = hv->b->sregs.ss.selector;
    prstatus->regs.ds = hv->b->sregs.ds.selector;
    prstatus->regs.es = hv->b->sregs.es.selector;
    prstatus->regs.fs = hv->b->sregs.fs.selector;
    prstatus->regs.gs = hv->b->sregs.gs.selector;
    prstatus->regs.fs_base = hv->b->sregs.fs.base;
    prstatus->regs.gs_base = hv->b->sregs.gs.base;
    prstatus->pid = getpid();

    /* Overwrite some register information based on
     * the input given by the Guest */
    if (info->len) {
        struct trap_regs *regs = (struct trap_regs *)info->data;
        prstatus->regs.rip = regs->rip;
        prstatus->regs.cs = regs->cs;
        prstatus->regs.eflags = regs->rflags;
        prstatus->regs.rsp = regs->rsp;
        prstatus->regs.ss = regs->ss;
    }
}

size_t ukvm_dumpcore_get_note_size(int *num_notes)
{
    *num_notes = 1;
    return (sizeof(Elf64_Nhdr) + 8 + sizeof(x86_elf_prstatus));
}

int ukvm_dumpcore_dump_notes(int core_fd,
        struct ukvm_hv *hv, struct ukvm_dumpcore *info)
{
    Elf64_Nhdr nhdr;
    x86_elf_prstatus prstatus;
    const char *name = "UNICORE";

    memset((void *)&nhdr, 0, sizeof(Elf64_Nhdr));
    memset((void *)&prstatus, 0, sizeof(x86_elf_prstatus));

    ukvm_hv_fill_elf_prstatus(&prstatus, hv, info);
    nhdr.n_namesz = strlen(name) + 1;
    nhdr.n_descsz = sizeof(x86_elf_prstatus);
    nhdr.n_type = NT_PRSTATUS;

    /* Write note */
    if (write(core_fd, &nhdr, sizeof(nhdr)) < 0) {
        return -1;
    }
    if (write(core_fd, name, nhdr.n_namesz) < 0) {
        return -1;
    }
    if (write(core_fd, &prstatus, sizeof(x86_elf_prstatus)) < 0) {
        return -1;
    }
    return 0;
}

int ukvm_dumpcore_get_regs(struct ukvm_hv *hv)
{
    if (ioctl(hv->b->vcpufd, KVM_GET_REGS, &hv->b->kregs) < 0) {
        warnx("KVM: Failed to get register info");
        return -1;
    }

    
    if (ioctl(hv->b->vcpufd, KVM_GET_SREGS, &hv->b->sregs) < 0) {
        warnx("KVM: Failed to get sregister info");
        return -1;
    }
    return 0;
}

