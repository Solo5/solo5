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

#include "../ukvm/kernel.h"
#include "../../ukvm/ukvm_guest.h"

void fpu_init(void)
{
    const unsigned default_mxcsr = 0x1f80;
    __asm__ __volatile__("ldmxcsr %0" : : "m"(default_mxcsr));
}

void platform_init(void *arg)
{
    process_bootinfo(arg);
    fpu_init();
}

void platform_exit(int status __attribute__((unused)))
{
    const char msg[] = "Solo5: Halted\n";
    platform_puts(msg, strlen(msg));
    __asm__ __volatile__("cli; hlt");
    for (;;);
}
