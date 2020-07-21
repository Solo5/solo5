/*
 * Copyright (c) 2015-2020 Contributors as noted in the AUTHORS file
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
 * Based on XTF hypercall-x86_64.h, which is:
 *
 * The Xen Test Framework is licenced under the BSD 2-clause license
 *
 * Copyright (c) 2014,2015 Citrix Systems Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __XEN_HYPERCALL_X86_64_H__
#define __XEN_HYPERCALL_X86_64_H__

/*
 * Hypercall primatives for 64bit
 *
 * Inputs: %rdi, %rsi, %rdx, %r10, %r8, %r9 (arguments 1-6)
 */

#define _hypercall64_1(type, hcall, a1)                                 \
    ({                                                                  \
        long __res, __ign1;                                             \
        __asm__ __volatile__ (                                          \
            "call HYPERCALL_PAGE + %c[offset]"                          \
            : "=a" (__res), "=D" (__ign1)                               \
            : [offset] "i" (hcall * 32),                                \
              "1" ((long)(a1))                                          \
            : "memory" );                                               \
        (type)__res;                                                    \
    })

#define _hypercall64_2(type, hcall, a1, a2)                             \
    ({                                                                  \
        long __res, __ign1, __ign2;                                     \
        __asm__ __volatile__ (                                          \
            "call HYPERCALL_PAGE + %c[offset]"                          \
            : "=a" (__res), "=D" (__ign1), "=S" (__ign2)                \
            : [offset] "i" (hcall * 32),                                \
              "1" ((long)(a1)), "2" ((long)(a2))                        \
            : "memory" );                                               \
        (type)__res;                                                    \
    })

#define _hypercall64_3(type, hcall, a1, a2, a3)                         \
    ({                                                                  \
        long __res, __ign1, __ign2, __ign3;                             \
        __asm__ __volatile__ (                                          \
            "call HYPERCALL_PAGE + %c[offset]"                          \
            : "=a" (__res), "=D" (__ign1), "=S" (__ign2), "=d" (__ign3) \
            : [offset] "i" (hcall * 32),                                \
              "1" ((long)(a1)), "2" ((long)(a2)), "3" ((long)(a3))      \
            : "memory" );                                               \
        (type)__res;                                                    \
    })

#define _hypercall64_4(type, hcall, a1, a2, a3, a4)                     \
    ({                                                                  \
        long __res, __ign1, __ign2, __ign3, __ign4;                     \
        register long _a4 __asm__ ("r10") = ((long)(a4));               \
        __asm__ __volatile__ (                                          \
            "call HYPERCALL_PAGE + %c[offset]"                          \
            : "=a" (__res), "=D" (__ign1), "=S" (__ign2), "=d" (__ign3),\
              "=&r" (__ign4)                                            \
            : [offset] "i" (hcall * 32),                                \
              "1" ((long)(a1)), "2" ((long)(a2)), "3" ((long)(a3)),     \
              "4" (_a4)                                                 \
            : "memory" );                                               \
        (type)__res;                                                    \
    })

#endif /* __XEN_HYPERCALL_X86_64_H__ */
