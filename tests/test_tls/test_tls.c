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

__thread volatile uint64_t data;

#if defined(__x86_64__)
	#define TLS_OFFSET sizeof(uint64_t)
#elif defined(__aarch64__)
	#define TLS_OFFSET (0 - 2*sizeof(void *))
#else
#error Unsupported architecture
#endif

volatile uint64_t data_thread_1 = 1;
volatile uint64_t data_thread_2 = 2;
volatile uint64_t data_thread_3 = 3;

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

int solo5_app_main(const struct solo5_start_info *si __attribute__((unused)))
{
    puts("\n**** Solo5 standalone test_tls ****\n\n");

    solo5_set_tls_base((uint64_t)&data_thread_1 + TLS_OFFSET);
    if (data != 1)
        return 1;

    solo5_set_tls_base((uint64_t)&data_thread_2 + TLS_OFFSET);
    if (data != 2)
        return 2;

    solo5_set_tls_base((uint64_t)&data_thread_3 + TLS_OFFSET);
    if (data != 3)
        return 3;

    solo5_set_tls_base((uint64_t)&data_thread_1 + TLS_OFFSET);
    data = 4;
    if (data_thread_1 != 4)
        return 4;

    solo5_set_tls_base((uint64_t)&data_thread_2 + TLS_OFFSET);
    data = 5;
    if (data_thread_2 != 5)
        return 5;

    solo5_set_tls_base((uint64_t)&data_thread_3 + TLS_OFFSET);
    data = 6;
    if (data_thread_3 != 6)
        return 6;

    puts("SUCCESS\n");
    return SOLO5_EXIT_SUCCESS;
}
