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
    puts("\n**** Solo5 standalone test_seccomp ****\n\n");

    /*
     * Try to call SYS_dup(1), which is not in any seccomp policy we use.
     */
#if defined(__x86_64__)
    int ret;

    __asm__ __volatile__ (
            "syscall"
            : "=a" (ret) : "a" (32), "D" (1) : "rcx", "r11", "memory"
    );
#elif defined(__aarch64__)
    register long x8 __asm__("x8") = 23;
    register long x0 __asm__("x0");
    register long x1 __asm__("x1") = 1;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (x0) : "r" (x8), "r" (x1) : "cc", "memory"
    );
#elif defined(__powerpc__)
    register long r0 __asm__("r0") = 41;
    register long r3 __asm__("r3") = 0;

    __asm__ __volatile__ (
            "sc"
            : "=r" (r3)
            : "r" (r0), "r" (r3)
            : "cc", "memory"
    );
#else
#error Unsupported architecture
#endif

    puts("FAILURE\n");
    return SOLO5_EXIT_FAILURE;
}
