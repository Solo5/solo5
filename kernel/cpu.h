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

#ifndef __CPU_H__
#define __CPU_H__

/* cpu.S: low-level CPU functions */
void cpu_halt(void) __attribute__((noreturn));
uint64_t cpu_rdtsc(void);

/* compiler-only memory "barrier" */
#define cpu_cc_barrier() __asm__ __volatile__("" : : : "memory")
#define cpu_mfence_memory() __asm__ ("mfence" ::: "memory");

static inline void cpu_wrmsr(uint64_t a, uint32_t b, uint32_t c)
{
    __asm__ __volatile("wrmsr" ::
                       "c" (a),
                       "a" (b),
                       "d" (c)
                       );
}

static inline void cpu_x86_cpuid(uint32_t level,
                                 uint32_t *eax_out, uint32_t *ebx_out,
                                 uint32_t *ecx_out, uint32_t *edx_out)
{
    uint32_t eax_, ebx_, ecx_, edx_;

    __asm__(
        "cpuid"
        : "=a" (eax_), "=b" (ebx_), "=c" (ecx_), "=d" (edx_)
        : "0" (level)
    );
    *eax_out = eax_;
    *ebx_out = ebx_;
    *ecx_out = ecx_;
    *edx_out = edx_;
}

static inline uint64_t cpu_mul64_32(uint64_t a, uint32_t b)
{
    uint64_t prod;

    __asm__ (
        "mul %%rdx ; "
        "shrd $32, %%rdx, %%rax"
        : "=a" (prod)
        : "0" (a), "d" ((uint64_t)b)
    );

    return prod;
}


#endif
