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

#include "solo5.h"
#include "../../kernel/lib.c"

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

int solo5_app_main(const struct solo5_start_info *si __attribute__((unused)))
{
    puts("\n**** Solo5 standalone test_fpu ****\n\n");

    float a, b, c[2];

    c[0] = 2.0;
    c[1] = 5.0;

#if defined(__x86_64__)
     __asm__ (
         "movups %0,%%xmm1;"
         "mulps %%xmm1, %%xmm1;"
         "movups %%xmm1, %0"
         : "=m" (c)
         : "m" (c)
         : "xmm1"
    );
#elif defined(__aarch64__)
    __asm__(
        "ldr d0, %0\n"
        "ldr d1, %0\n"
        "fmul v0.2s, v1.2s, v0.2s\n"
        "str d0, %0\n"
        : "=m" (c)
        : "m" (c)
    );
#else
#error Unsupported architecture
#endif

    a = 1.5;
    b = 5.0;
    a *= b;

    if (a == 7.5 && c[0] == 4.0 && c[1] == 25.0)
        puts("SUCCESS\n");
    else
        return SOLO5_EXIT_FAILURE;

    return SOLO5_EXIT_SUCCESS;
}
