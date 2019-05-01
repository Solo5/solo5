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
#ifndef __CPU_AARCH64_H__
#define __CPU_AARCH64_H__

/* memory defines */
#define PAGE_SIZE   4096
#define PAGE_SHIFT  12
#define PAGE_MASK   ~(0xfff)

#ifndef _BITUL

#ifdef ASM_FILE
#define _AC(X,Y)    X
#define _AT(T,X)    X
#else
#define __AC(X,Y)   (X##Y)
#define _AC(X,Y)    __AC(X,Y)
#define _AT(T,X)    ((T)(X))
#endif

#define _BITUL(x)   (_AC(1,UL) << (x))
#define _BITULL(x)  (_AC(1,ULL) << (x))

#endif

#define ESR_EC_IABT_LOW	_AC(0x20, UL)
#define ESR_EC_IABT_CUR	_AC(0x21, UL)
#define ESR_EC_DABT_LOW	_AC(0x24, UL)
#define ESR_EC_DABT_CUR	_AC(0x25, UL)

#define ESR_EC_SHIFT    _AC(26, UL)
#define ESR_EC_MASK     (_AC(0x3F, UL) << ESR_EC_SHIFT)
#define ESR_EC(esr)     (((esr) & ESR_EC_MASK) >> ESR_EC_SHIFT)

#ifndef ASM_FILE

/*
 * The remainder of this file is used only from C.
 */
static inline uint64_t cpu_cntvct(void)
{
    uint64_t val;

    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r" (val)::);
    return val;
}

static inline uint64_t mul64_32(uint64_t a, uint32_t b, uint8_t s)
{
    return (a * b) >> s;
}
#endif /* !ASM_FILE */

static inline void cpu_set_tls_base(uint64_t base)
{
    __asm__ __volatile("msr tpidr_el0, %0" :: "r"(base));
}

#endif /* __CPU_AARCH64_H__ */
