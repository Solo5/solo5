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

extern uintptr_t SSP_GUARD;

#if defined(__x86_64__)
#define READ_CPU_TICKS cpu_rdtsc
#elif defined(__aarch64__)
#define READ_CPU_TICKS cpu_cntvct
#elif defined(__powerpc64__)
#define READ_CPU_TICKS cpu_cntvct
#else
#error Unsupported architecture
#endif

/*
 * This function must be inlined as early as possible once in C code, before
 * calling any other functions. The calling function must not return.
 */

__attribute__((always_inline)) static inline void crt_init_ssp(void)
{
    /*
     * Initialise the stack canary value.
     */
    SSP_GUARD = READ_CPU_TICKS() + (READ_CPU_TICKS() << 32UL);
    SSP_GUARD &= ~(uintptr_t)0xff00;
}

/*
 * Explicitly disable any accidental use of TLS by setting the arch-specific
 * TLS base to zero, thus causing TLS access to point to the zero page.
 */

__attribute__((always_inline)) static inline void crt_init_tls(void)
{
    (void)platform_set_tls_base(0);
}
