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

#include "kernel.h"

void _start(struct ukvm_boot_info *bi)
{
    int ret;

    printf("            |      ___|\n");
    printf("  __|  _ \\  |  _ \\ __ \\\n");
    printf("\\__ \\ (   | | (   |  ) |\n");
    printf("____/\\___/ _|\\___/____/\n");

    gdt_init();
    mem_init(bi->mem_size, bi->kernel_end);
    intr_init();

    /* for floating point */
    cpu_sse_enable();
    time_init();

    intr_enable();

    ret = solo5_app_main((char *)bi->cmdline);
    printf("Solo5: solo5_app_main() returned with %d\n", ret);

    platform_exit();
}
