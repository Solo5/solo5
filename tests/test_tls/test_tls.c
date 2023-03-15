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

uintptr_t tcb1;
uintptr_t tcb2;

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

/* This just stub a libc malloc setup with a limited 64k heap.
 * This does not guard heap overwritting on stack, it's just a testfile.
 */
#define heap_size 65536
static uint8_t heap[heap_size];
static uintptr_t heap_top = (uintptr_t)&heap;

static void* malloc_stub(size_t size)
{
    if (heap_top + size <= (uintptr_t)&heap + heap_size)
    {
        heap_top += size;
        return (void*)(heap_top - size);
    }

    return NULL;
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

__thread volatile uint64_t _data_not_bss = 0x0000000000C0FFEE;
uint64_t __attribute__ ((noinline)) get_data_not_bss()
{
    return _data_not_bss;
}

int solo5_app_main(const struct solo5_start_info *si __attribute__((unused)))
{
    puts("\n**** Solo5 standalone test_tls ****\n\n");

    tcb1 = (uintptr_t)malloc_stub(solo5_tls_size());
    if (solo5_tls_init(tcb1) != SOLO5_R_OK)
        return 10;
 
    tcb2 = (uintptr_t)malloc_stub(solo5_tls_size());
    if (solo5_tls_init(tcb2) != SOLO5_R_OK)
        return 11;
 
    if (solo5_set_tls_base(solo5_tls_tp_offset(tcb1)) != SOLO5_R_OK)
        return 1;
    set_data(1);

    if (solo5_set_tls_base(solo5_tls_tp_offset(tcb2)) != SOLO5_R_OK)
        return 2;
    set_data(2);

    if (solo5_set_tls_base(solo5_tls_tp_offset(tcb1)) != SOLO5_R_OK)
        return 3;
    if (get_data() != 1)
        return 4;

    if (solo5_set_tls_base(solo5_tls_tp_offset(tcb2)) != SOLO5_R_OK)
        return 5;
    if (get_data() != 2)
        return 6;

    if (get_data_not_bss() != 0x0000000000C0FFEE)
        return 7;

    puts("SUCCESS\n");
    return SOLO5_EXIT_SUCCESS;
}
