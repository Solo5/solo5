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
#include "../crt_init.h"

extern void _newstack(uint64_t stack_start, void (*tramp)(void *), void *arg);
static void _start2(void *arg) __attribute__((noreturn));

void _start(void *arg)
{
    uint64_t tls_base;

    crt_init_ssp();

    volatile int gdb = 1;

    serial_init();

    if (!gdb)
        log(INFO, "Solo5: Waiting for gdb...\n");
    while (gdb == 0)
        ;

    cpu_init();
    platform_init(arg, &tls_base);
    crt_init_tls(tls_base);

    /*
     * Switch away from the bootstrap stack (in boot.S) as early as possible.
     */
    _newstack(platform_mem_size(), _start2, 0);
}

static void _start2(void *arg __attribute__((unused)))
{
    static struct solo5_start_info si;

    si.cmdline = cmdline_parse(platform_cmdline());

    log(INFO, "            |      ___|\n");
    log(INFO, "  __|  _ \\  |  _ \\ __ \\\n");
    log(INFO, "\\__ \\ (   | | (   |  ) |\n");
    log(INFO, "____/\\___/ _|\\___/____/\n");

    mem_init();
    time_init();
    pci_enumerate();
    cpu_intr_enable();

    mem_lock_heap(&si.heap_start, &si.heap_size);
    solo5_exit(solo5_app_main(&si));
}
