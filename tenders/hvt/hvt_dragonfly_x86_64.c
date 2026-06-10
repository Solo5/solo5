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
 * hvt_dragonfly_x86_64.c: x86_64 architecture-dependent part of DragonFly
 * nvmm(4) backend implementation.
 */

#define _GNU_SOURCE
#include <err.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <nvmm.h>

#include "hvt.h"
#include "hvt_dragonfly.h"
#include "hvt_cpu_x86_64.h"

static struct hvt *global_hvt;
static int exitcode = 0;
static int halted = 0;

static void io_callback(struct nvmm_io *io)
{
    struct hvt_b *hvb = global_hvt->b;

    if (io->port == HVT_RING_KICK_PIO_BASE && !io->in && io->size == 4) {
        if (hvb->kick_net_pipe[1] != -1) {
            uint8_t byte = 1;
            write(hvb->kick_net_pipe[1], &byte, 1);
        }
        return;
    }
    if (io->in || io->size != 4)
        errx(EXIT_FAILURE, "Invalid guest port access: port=0x%x", io->port);
    if (io->port < HVT_HYPERCALL_PIO_BASE ||
        io->port >= (HVT_HYPERCALL_PIO_BASE + HVT_HYPERCALL_MAX))
        errx(EXIT_FAILURE, "Invalid guest port access: port=0x%x", io->port);

    int nr = io->port - HVT_HYPERCALL_PIO_BASE;

    hvt_gpa_t gpa = io->vcpu->state->gprs[NVMM_X64_GPR_RAX];

    /* Guest has halted the CPU. */
    if (nr == HVT_HYPERCALL_HALT) {
        exitcode = hvt_core_hypercall_halt(global_hvt, gpa);
        halted = 1;
        return;
    }

    hvt_hypercall_fn_t fn = hvt_core_hypercalls[nr];
    if (fn == NULL)
        errx(EXIT_FAILURE, "Invalid guest hypercall: num=%d", nr);

    fn(global_hvt, gpa);
}

static struct nvmm_assist_callbacks callbacks = {.io = io_callback,
                                                 .mem = NULL};

void hvt_mem_size(size_t *mem_size)
{
    hvt_x86_mem_size(mem_size);
}

void hvt_mem_size_roundup(size_t *mem_size)
{
    size_t mem = ((*mem_size + X86_GUEST_PAGE_SIZE - 1) / X86_GUEST_PAGE_SIZE) *
                 X86_GUEST_PAGE_SIZE;
    if (mem > X86_GUEST_MAX_MEM_SIZE)
        mem = X86_GUEST_MAX_MEM_SIZE;
    *mem_size = mem;
}

static void set_seg(struct nvmm_x64_state_seg *seg, const struct x86_sreg *sreg)
{
    seg->selector = sreg->selector * 8;
    seg->attrib.type = sreg->type;
    seg->attrib.s = sreg->s;
    seg->attrib.dpl = sreg->dpl;
    seg->attrib.p = sreg->p;
    seg->attrib.avl = sreg->avl;
    seg->attrib.l = sreg->l;
    seg->attrib.def = sreg->db;
    seg->attrib.g = sreg->g;
    seg->attrib.rsvd = sreg->unusable;
    seg->limit = sreg->limit;
    seg->base = sreg->base;
}

void hvt_vcpu_init(struct hvt *hvt, hvt_gpa_t gpa_ep)
{
    struct hvt_b *hvb = hvt->b;
    struct nvmm_vcpu *vcpu = &hvb->vcpu;

    hvt_x86_setup_gdt(hvt->mem);
    hvt_x86_setup_pagetables(hvt->mem, hvt->mem_alloc_size);

    if (nvmm_vcpu_getstate(&hvb->mach, vcpu, NVMM_X64_STATE_ALL) == -1)
        err(EXIT_FAILURE, "unable to get VCPU state");

    vcpu->state->crs[NVMM_X64_CR_CR0] = X86_CR0_INIT;
    vcpu->state->crs[NVMM_X64_CR_CR3] = X86_CR3_INIT;
    vcpu->state->crs[NVMM_X64_CR_CR4] = X86_CR4_INIT;
    vcpu->state->msrs[NVMM_X64_MSR_EFER] = X86_EFER_INIT;
    vcpu->state->gprs[NVMM_X64_GPR_RIP] = gpa_ep;
    vcpu->state->gprs[NVMM_X64_GPR_RFLAGS] = X86_RFLAGS_INIT;
    vcpu->state->gprs[NVMM_X64_GPR_RSP] = hvt->guest_mem_size - 8;
    vcpu->state->gprs[NVMM_X64_GPR_RDI] = X86_BOOT_INFO_BASE;

    set_seg(&vcpu->state->segs[NVMM_X64_SEG_CS], &hvt_x86_sreg_code);
    set_seg(&vcpu->state->segs[NVMM_X64_SEG_SS], &hvt_x86_sreg_data);
    set_seg(&vcpu->state->segs[NVMM_X64_SEG_DS], &hvt_x86_sreg_data);
    set_seg(&vcpu->state->segs[NVMM_X64_SEG_ES], &hvt_x86_sreg_data);
    set_seg(&vcpu->state->segs[NVMM_X64_SEG_FS], &hvt_x86_sreg_data);
    set_seg(&vcpu->state->segs[NVMM_X64_SEG_GS], &hvt_x86_sreg_data);
    set_seg(&vcpu->state->segs[NVMM_X64_SEG_TR], &hvt_x86_sreg_tr);
    set_seg(&vcpu->state->segs[NVMM_X64_SEG_LDT], &hvt_x86_sreg_unusable);

    const struct x86_sreg hvt_x86_sreg_gdt = {.base = X86_GDT_BASE,
                                              .limit = X86_GDTR_LIMIT};
    set_seg(&vcpu->state->segs[NVMM_X64_SEG_GDT], &hvt_x86_sreg_gdt);

    nvmm_vcpu_setstate(&hvb->mach, vcpu, NVMM_X64_STATE_ALL);

    uint64_t cpu_cycle_freq;
    size_t outsz = sizeof(cpu_cycle_freq);
    int ret =
        sysctlbyname("hw.tsc_frequency", &cpu_cycle_freq, &outsz, NULL, 0);
    if (ret == -1)
        err(EXIT_FAILURE, "sysctl(hw.tsc_frequency)");
    int invariant_tsc = 0;
    outsz = sizeof(invariant_tsc);
    ret = sysctlbyname("hw.tsc_invariant", &invariant_tsc, &outsz, NULL, 0);
    if (ret == -1)
        err(EXIT_FAILURE, "sysctl(hw.tsc_invariant");
    if (invariant_tsc != 1)
        errx(EXIT_FAILURE, "Host TSC is not invariant, cannot continue");
    hvt->cpu_cycle_freq = cpu_cycle_freq;

    hvt->cpu_boot_info_base = X86_BOOT_INFO_BASE;

    // we need a global here for use within io_callback().
    global_hvt = hvt;

    if (nvmm_vcpu_configure(&hvb->mach, &hvb->vcpu, NVMM_VCPU_CONF_CALLBACKS,
                            &callbacks) == -1)
        err(EXIT_FAILURE, "nvmm_vcpu_configure");
}

int hvt_vcpu_loop(struct hvt *hvt)
{
    struct hvt_b *hvb = hvt->b;
    struct nvmm_machine *mach = &hvb->mach;
    struct nvmm_vcpu *vcpu = &hvb->vcpu;
    int ret;

    while (1) {
        if (nvmm_vcpu_run(mach, vcpu) == -1)
            err(EXIT_FAILURE, "unable to run VCPU");

        /* Process the exit reasons. */
        switch (vcpu->exit->reason) {
        case NVMM_VCPU_EXIT_NONE:
            break;
        case NVMM_VCPU_EXIT_HALTED:
            return 0;
        case NVMM_VCPU_EXIT_IO:
            ret = nvmm_assist_io(mach, vcpu);
            if (ret == -1)
                err(EXIT_FAILURE, "nvmm_assist_io");
            if (halted)
                return (exitcode);
            continue;
        default:
            errx(EXIT_FAILURE, "unknown exit reason: 0x%lx",
                 vcpu->exit->reason);
        }
    }
    return 0;
}
