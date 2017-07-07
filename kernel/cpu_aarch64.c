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
