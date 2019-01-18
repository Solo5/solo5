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

extern uintptr_t __stack_chk_guard;

#if defined(__x86_64__)
#define READ_CPU_TICKS cpu_rdtsc
#elif defined(__aarch64__)
#define READ_CPU_TICKS cpu_cntvct
#else
#error Unsupported architecture
#endif

/*
 * These functions are responsible for performing early initialisation required
 * by the "C runtime". They must be inlined as early as possible once in C
 * code, before calling any other functions. The calling function must not
 * return.
 *
 * As crt_init_tls() currently requires that the bindings are running in CPU
 * Ring 0 or equivalent, this is split into a separate function so that
 * bindings such as "spt" can omit the call.
 */

__attribute__((always_inline)) static inline void crt_init_tls(void)
{
    /*
     * Explicitly disable any accidental use of TLS by zeroing the relevant
     * CPU registers, thus causing TLS access to point to the zero page.
     *
     * On x86_64 these MSRs "should" technically be zero on hvt VM entry, but
     * this is not actually specified anywhere. For virtio the bootloader
     * and/or platform may have likewise clobbered the values.
     *
     * On aarch64 the defaults seen in practice on KVM appear to be garbage
     * values. Note that we are running at EL1, but based on practical
     * investigation TPIDR_EL0 is the register that the compiler would use to
     * point to TLS.
     */
#if defined(__x86_64__)
     __asm__ __volatile("wrmsr" ::
         "c" (0xc0000100), /* IA32_FS_BASE */
         "a" (0x0),
         "d" (0x0)
     );
     __asm__ __volatile("wrmsr" ::
         "c" (0xc0000101), /* IA32_GS_BASE */
         "a" (0x0),
         "d" (0x0)
     );
#elif defined(__aarch64__)
     __asm__ __volatile("msr tpidr_el0, xzr");
#else
#error Unsupported architecture
#endif
}

__attribute__((always_inline)) static inline void crt_init_ssp(void)
{
    /*
     * Initialise the stack canary value.
     */
    __stack_chk_guard = READ_CPU_TICKS() + (READ_CPU_TICKS() << 32UL);
    __stack_chk_guard &= ~(uintptr_t)0xff00;
}
