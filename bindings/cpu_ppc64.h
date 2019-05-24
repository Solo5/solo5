/*
 * Copyright (c) 2019 Contributors as noted in the AUTHORS file
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
#ifndef __CPU_PPC64_H__
#define __CPU_PPC64_H__

/* memory defines */
#define PAGE_SIZE   (64 * 1024)
#define PAGE_SHIFT  16
#define PAGE_MASK   ~(0xffff)

#ifndef ASM_FILE

/*
 * The remainder of this file is used only from C.
 */
static inline uint64_t cpu_cntvct(void)
{
    uint64_t val;

    __asm__ __volatile__(
        "mfspr %0, 268\n"
        : "=r" (val)
        :
        :);

    return val;
}

static inline void cpu_set_tls_base(uint64_t base)
{
    __asm__ __volatile(
        "mr 13, %0\n"
        :
        : "a" (base)
        : "r13");
}

#endif /* !ASM_FILE */

#endif /* __CPU_PPC64_H__ */
