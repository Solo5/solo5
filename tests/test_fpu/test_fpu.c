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

#include "solo5.h"
#include "../../bindings/lib.c"

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

int solo5_app_main(const struct solo5_start_info *si __attribute__((unused)))
{
    puts("\n**** Solo5 standalone test_fpu ****\n\n");

    float a, b, c[4];

    c[0] = 2.0;
    c[1] = 5.0;
    c[2] = 3.0;
    c[3] = 8.0;

#if defined(__x86_64__)
     __asm__ (
         "movaps %0,%%xmm1;"
         "mulps %%xmm1, %%xmm1;"
         "movaps %%xmm1, %0"
         : "=m" (c)
         : "m" (c)
         : "xmm1"
    );
#elif defined(__aarch64__)
    __asm__(
        "ldr q0, %0\n"
        "ldr q1, %0\n"
        "fmul v0.4s, v1.4s, v0.4s\n"
        "str q0, %0\n"
        : "=m" (c)
        : "m" (c)
        : "q0", "q1", "v0"
    );
#elif defined(__powerpc64__)
#define DOMUL(VAR) 			\
    __asm__(				\
        "lfs %%f0, %0\n"		\
        "fmuls %%f0, %%f0, %%f0\n"	\
        "stfs %%f0, %0\n"		\
        : "=m" (VAR)			\
        : "m" (VAR)			\
        : "memory"			\
    )
    DOMUL(c[0]);
    DOMUL(c[1]);
    DOMUL(c[2]);
    DOMUL(c[3]);
#undef DOMUL
#else
#error Unsupported architecture
#endif

    a = 1.5;
    b = 5.0;
    a *= b;

    if (a == 7.5 && c[0] == 4.0 && c[1] == 25.0 && c[2] == 9.0 && c[3] == 64.0)
        puts("SUCCESS\n");
    else
        return SOLO5_EXIT_FAILURE;

    return SOLO5_EXIT_SUCCESS;
}
