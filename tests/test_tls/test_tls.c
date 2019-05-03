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

volatile long data_thread_1 = 0;
volatile long data_thread_2 = 0;

static void tls_store_long(long data)
{
#if defined(__x86_64__)
    __asm__ __volatile("mov %0, %%fs:0x0" : : "r" (data));
#elif defined(__aarch64__)
    __asm__ __volatile("mrs x0, tpidr_el0; "
                       "str %0, [x0]"
                       : : "r" (data) : "x0");
#else
#error Unsupported architecture
#endif
}

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

int solo5_app_main(const struct solo5_start_info *si __attribute__((unused)))
{
    puts("\n**** Solo5 standalone test_tls ****\n\n");

    if (solo5_set_tls_base((uintptr_t)&data_thread_1) != SOLO5_R_OK)
        return 1;
    tls_store_long(-12345);

    if (solo5_set_tls_base((uintptr_t)&data_thread_2) != SOLO5_R_OK)
        return 2;
    tls_store_long(56789);

    if (data_thread_1 != -12345)
        return 3;

    if (data_thread_2 != 56789)
        return 4;

    puts("SUCCESS\n");
    return SOLO5_EXIT_SUCCESS;
}
