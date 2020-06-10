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

#include "bindings.h"

static const char *cmdline;
#if defined(__BITS_32__)
static uint32_t mem_size;
#elif defined(__BITS_64__)
static uint64_t mem_size;
#else
#error Unsupported architecture bits
#endif

void platform_init(const void *arg)
{
    const struct spt_boot_info *bi = arg;

    cmdline = bi->cmdline;
    mem_size = bi->mem_size;
}

const char *platform_cmdline(void)
{
    return cmdline;
}

uint64_t platform_mem_size(void)
{
    return mem_size;
}

void platform_exit(int status, void *cookie __attribute__((unused)))
{
    sys_exit_group(status);
}

int platform_puts(const char *buf, int n)
{
    (void)sys_write(SYS_STDOUT, buf, n);
    return n;
}

#if defined(__BITS_32__)
int platform_set_tls_base(uint32_t base)
#elif defined(__BITS_64__)
int platform_set_tls_base(uint64_t base)
#else
#error Unsupported architecture bits
#endif
{
#if defined(__x86_64__)
    /* In x86 we need to ask the host kernel to change %fs for us. */
    return sys_arch_prctl(SYS_ARCH_SET_FS, base);
#elif defined(__aarch64__) || defined(__arm__)
    cpu_set_tls_base(base);
    return 0;
#elif defined(__powerpc64__)
    cpu_set_tls_base(base);
    return 0;
#else
#error Unsupported architecture
#endif
}
