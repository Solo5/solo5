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


/* Structure definitions copied from qemu source code at target/i386/arch_dump.c */

typedef struct {
    uint64_t cr2;
    uint64_t ec;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} x86_64_trap_regs_struct;

typedef struct {
    uint64_t r15, r14, r13, r12, rbp, rbx, r11, r10;
    uint64_t r9, r8, rax, rcx, rdx, rsi, rdi, orig_rax;
    uint64_t rip, cs, eflags;
    uint64_t rsp, ss;
    uint64_t fs_base, gs_base;
    uint64_t ds, es, fs, gs;
} x86_64_user_regs_struct;

typedef struct {
    char pad1[32];
    uint32_t pid;
    char pad2[76];
    x86_64_user_regs_struct regs;
    char pad3[8];
} x86_elf_prstatus;

static void ukvm_hv_fill_elf_prstatus(x86_elf_prstatus *prstatus,
	                              struct ukvm_hv *hv, void *cookie)
{
    struct kvm_regs  kregs;
    struct kvm_sregs sregs;

    if (ioctl(hv->b->vcpufd, KVM_GET_SREGS, &sregs) < 0) {
        warnx("KVM: Failed to get sregister info");
        return;
    }

    if (ioctl(hv->b->vcpufd, KVM_GET_REGS, &kregs) < 0) {
        warnx("KVM: Failed to get kregister info");
        return;
    }

    memset(prstatus, 0, sizeof(x86_elf_prstatus));
    prstatus->regs.r8 = kregs.r8;
    prstatus->regs.r9 = kregs.r9;
    prstatus->regs.r10 = kregs.r10;
    prstatus->regs.r11 = kregs.r11;
    prstatus->regs.r12 = kregs.r12;
    prstatus->regs.r13 = kregs.r13;
    prstatus->regs.r14 = kregs.r14;
    prstatus->regs.r15 = kregs.r15;
    prstatus->regs.rbp = kregs.rbp;
    prstatus->regs.rsp = kregs.rsp;
    prstatus->regs.rdi = kregs.rdi;
    prstatus->regs.rsi = kregs.rsi;
    prstatus->regs.rdx = kregs.rdx;
    prstatus->regs.rcx = kregs.rcx;
    prstatus->regs.rbx = kregs.rbx;
    prstatus->regs.rax = kregs.rax;
    prstatus->regs.rip = kregs.rip;
    prstatus->regs.eflags = kregs.rflags;

    prstatus->regs.cs = sregs.cs.selector;
    prstatus->regs.ss = sregs.ss.selector;
    prstatus->regs.ds = sregs.ds.selector;
    prstatus->regs.es = sregs.es.selector;
    prstatus->regs.fs = sregs.fs.selector;
    prstatus->regs.gs = sregs.gs.selector;
    prstatus->regs.fs_base = sregs.fs.base;
    prstatus->regs.gs_base = sregs.gs.base;
    prstatus->pid = getpid();

    /* Overwrite some register information based on
     * the input given by the Guest */
    if (cookie) {
        assert(sizeof(x86_64_trap_regs_struct) < UKVM_COOKIE_MAX);
        x86_64_trap_regs_struct *regs = (x86_64_trap_regs_struct *)cookie;
        prstatus->regs.rip = regs->rip;
        prstatus->regs.cs = regs->cs;
        prstatus->regs.eflags = regs->rflags;
        prstatus->regs.rsp = regs->rsp;
        prstatus->regs.ss = regs->ss;
    }
}

int ukvm_is_arch_supported()
{
    return 1;
}

size_t ukvm_dumpcore_get_note_size(int *num_notes)
{
    *num_notes = 1;
    return (sizeof(Elf64_Nhdr) + 8 + sizeof(x86_elf_prstatus));
}

int ukvm_dumpcore_dump_notes(int core_fd, struct ukvm_hv *hv, void *cookie)
{
    Elf64_Nhdr nhdr;
    x86_elf_prstatus prstatus;
    const char *name = "UNIDUMP";

    memset((void *)&nhdr, 0, sizeof(Elf64_Nhdr));
    memset((void *)&prstatus, 0, sizeof(x86_elf_prstatus));

    ukvm_hv_fill_elf_prstatus(&prstatus, hv, cookie);
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

void ukvm_dumpcore_fill_arch_header(Elf64_Ehdr *hdr)
{
    hdr->e_ident[EI_CLASS] = ELFCLASS64;
    hdr->e_ident[EI_DATA] = ELFDATA2LSB; 
}
