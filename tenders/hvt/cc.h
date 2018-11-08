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
 * hvt_cc.h: Compiler support macros.
 */

#ifndef HVT_CC_H
#define HVT_CC_H

#if defined(__GNUC__)
#if __GNUC__ >= 5
#define _HAS_BUILTIN_ADD_OVERFLOW
#endif
#endif

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if __has_builtin(__builtin_add_overflow)
#define _HAS_BUILTIN_ADD_OVERFLOW
#endif

#ifdef _HAS_BUILTIN_ADD_OVERFLOW

/*
 * Compiler supports __builtin_add_overflow(), use it for checked arithmetic.
 */

#define add_overflow(a,b,r) __builtin_add_overflow(a,b,&r)

#else /* !_HAS_BUILTIN_ADD_OVERFLOW */

/*
 * Define add_overflow(a, b, result) for compilers without
 * __builtin_add_overlow().  Based on "Catching Integer Overflows in C"
 * (https://www.fefe.de/intof.html).
 */

#define __HALF_MAX_SIGNED(type) ((type)1 << (sizeof(type) * 8 - 2))
#define __MAX_SIGNED(type) (__HALF_MAX_SIGNED(type) - 1 + __HALF_MAX_SIGNED(type))
#define __MIN_SIGNED(type) (-1 - __MAX_SIGNED(type))

#define __MIN(type) ((type)-1 < 1 ? __MIN_SIGNED(type) : (type)0)
#define __MAX(type) ((type)~__MIN(type))

#define __assign(dest,src)                                                     \
    ({                                                                         \
        __typeof(src) __x = (src);                                             \
        __typeof(dest) __y = __x;                                              \
        (__x == __y && ((__x < 1) == (__y < 1)) ? (void)((dest) = __y),0 : 1); \
    })

#define add_overflow(a,b,r)                                                    \
    ({                                                                         \
    __typeof(a) __a = a;                                                       \
    __typeof(b) __b = b;                                                       \
    (__b) < 1 ?                                                                \
    ((__MIN(__typeof(r)) - (__b) <= (__a)) ? __assign(r, __a + __b) : 1) :     \
    ((__MAX(__typeof(r)) - (__b) >= (__a)) ? __assign(r, __a + __b) : 1);      \
    })

#endif /* _HAS_BUILTIN_ADD_OVERFLOW */
#undef _HAS_BUILTIN_ADD_OVERFLOW

#endif /* HVT_CC_H */
