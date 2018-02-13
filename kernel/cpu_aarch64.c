/*
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a unikernel base layer.
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

#include "ukvm/kernel.h"

extern void *cpu_exception_vectors;

struct regs {
    uint64_t xreg[30];
    uint64_t lr;
    uint64_t elr_el1;
    uint64_t spsr_el1;
    uint64_t esr_el1;
};

static const char *exception_modes[]= {
    "Synchronous Abort",
    "IRQ",
    "FIQ",
    "Error"
};

void cpu_init(void)
{
    __asm__ __volatile__("msr VBAR_EL1, %0"
            :
            : "r" ((uint64_t)&cpu_exception_vectors)
            : "memory");
}

static void dump_registers(struct regs *regs)
{
    uint32_t idx;

    log(INFO, "Solo5: Dump registers:\n");
    log(INFO, "\t ESR    : 0x%016lx\n", regs->esr_el1);
    log(INFO, "\t PC     : 0x%016lx\n", regs->elr_el1);
    log(INFO, "\t LR     : 0x%016lx\n", regs->lr);
    log(INFO, "\t PSTATE : 0x%016lx\n", regs->spsr_el1);

    for (idx = 0; idx < 28; idx+=4)
        log(INFO, "\t x%02d ~ x%02d: 0x%016lx 0x%016lx 0x%016lx 0x%016lx\n",
            idx, idx + 3, regs->xreg[idx], regs->xreg[idx + 1],
            regs->xreg[idx + 2], regs->xreg[idx + 3]);

    log(INFO, "\t x28 ~ x29: 0x%016lx 0x%016lx\n", regs->xreg[28], regs->xreg[29]);
}

void cpu_trap_handler(struct regs *regs, int el, int mode, int is_valid)
{
    log(INFO, "Solo5: Trap: EL%d %s%s caught\n",
        el, is_valid ? "" : "Invalid ", exception_modes[mode], el);

    dump_registers(regs);

    PANIC("PANIC");
}

/* keeps track of cpu_intr_disable() depth */
int cpu_intr_depth = 1;

void cpu_intr_disable(void)
{
    __asm__ __volatile__("msr daifset, #2");
    cpu_intr_depth++;
}

void cpu_intr_enable(void)
{
    assert(cpu_intr_depth > 0);

    if (--cpu_intr_depth == 0)
        __asm__ __volatile__("msr daifclr, #2");
}

void cpu_halt(void)
{
    /* Copied from FreeBSD:sys/arm64/arm64/machdep.c */
    cpu_intr_disable();
    while (1) {
        __asm __volatile("wfi");
    }
}
