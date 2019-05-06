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
struct __attribute__((aligned(8),packed))  tcb {
    volatile uint64_t data;
    void *tp;
};
#elif defined(__aarch64__)
struct __attribute__((aligned(8),packed))  tcb {
    void *tp;
    void *pad;
    volatile uint64_t data;
};
#else
#error Unsupported architecture
#endif

struct tcb tcb1;
struct tcb tcb2;

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

int solo5_app_main(const struct solo5_start_info *si __attribute__((unused)))
{
    puts("\n**** Solo5 standalone test_tls ****\n\n");

    memset(&tcb1, sizeof(tcb1), 0);
    memset(&tcb2, sizeof(tcb2), 0);

#if defined(__x86_64__)
    if (sizeof(struct tcb) != (8 * 2))
        return 7;
#else
    if (sizeof(struct tcb) != (8 * 3))
        return 8;
#endif

    tcb1.data = 1;
    solo5_set_tls_base((uint64_t)&tcb1.tp);
    if (data != 1)
        return 1;

    tcb2.data = 2;
    solo5_set_tls_base((uint64_t)&tcb2.tp);
    if (data != 2)
        return 2;

    solo5_set_tls_base((uint64_t)&tcb1.tp);
    data = 4;
    if (tcb1.data != 4)
        return 4;

    solo5_set_tls_base((uint64_t)&tcb2.tp);
    data = 5;
    if (tcb2.data != 5)
        return 5;

    puts("SUCCESS\n");
    return SOLO5_EXIT_SUCCESS;
}
