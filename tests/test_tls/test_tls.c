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

/*
 * XXX TODO: This test contains a bad, incomplete implementation of TLS.
 * It's unclear what we should be testing here, and what "TLS support" in
 * the context of Solo5 means at all.
 */

/*
 * FreeBSD clang toolchain post 8.0.1/12.1-RELEASE fails to build this test
 * with:
 *
 * ld: error: test_tls.o has an STT_TLS symbol but doesn't have an SHF_TLS section
 *
 * OpenBSD clang toolchains have unspecified non-support for TLS.
 *
 * In both cases just compile a dummy and disable this test in tests.bats.
 */
/* XXX The above actually applies to any toolchain using lld. Either disable
 * this test entirely or fix the linker scripts? */
#if defined(__clang__)

int solo5_app_main(const struct solo5_start_info *si __attribute__((unused)))
{
    return SOLO5_EXIT_FAILURE;
}

#else

#if defined(__x86_64__) || defined(__powerpc64__)
/* Variant II */
struct tcb {
    volatile uint64_t _data;
    void *tp;
};

#define PPC64_TLS_OFFSET 0x7000

#elif defined(__aarch64__)
/* Variant I */
struct tcb {
    void *tp;
    void *pad;
    volatile uint64_t _data;
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

__thread volatile uint64_t _data;

uint64_t __attribute__ ((noinline)) get_data()
{
    return _data;
}

void __attribute__ ((noinline)) set_data(uint64_t data)
{
    _data = data;
}

int solo5_app_main(const struct solo5_start_info *si __attribute__((unused)))
{
    puts("\n**** Solo5 standalone test_tls ****\n\n");

#if defined (__powerpc64__)
    tcb1.tp = (void *)&tcb1._data + PPC64_TLS_OFFSET;
    tcb2.tp = (void *)&tcb2._data + PPC64_TLS_OFFSET;
#else
    tcb1.tp = &tcb1.tp;
    tcb2.tp = &tcb2.tp;
#endif

    if (solo5_set_tls_base((uintptr_t)tcb1.tp) != SOLO5_R_OK)
        return 1;
    set_data(1);

    if (solo5_set_tls_base((uintptr_t)tcb2.tp) != SOLO5_R_OK)
        return 2;
    set_data(2);

    if (solo5_set_tls_base((uintptr_t)tcb1.tp) != SOLO5_R_OK)
        return 3;
    if (get_data() != 1)
        return 4;

    if (solo5_set_tls_base((uintptr_t)tcb2.tp) != SOLO5_R_OK)
        return 5;
    if (get_data() != 2)
        return 6;

    puts("SUCCESS\n");
    return SOLO5_EXIT_SUCCESS;
}

#endif /* defined(__clang__) */
