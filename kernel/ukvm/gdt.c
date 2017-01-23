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

/* granularity (23), long mode (21), present (15),
 * always 1 (12, 11), readable (9), limit (16-19)
 */
#define GDT_DESC_CODE_VAL (0x00af9a000000ffff)
/* granularity (23), big data seg (22), present (15),
 * type data rw (9), limit (16-19)
 */
#define GDT_DESC_DATA_VAL (0x00cf92000000ffff)

struct __attribute__((__packed__)) gdtptr {
    uint16_t limit;
    uint64_t base;
};

uint64_t cpu_gdt64[GDT_NUM_ENTRIES] ALIGN_64_BIT;

/*
 * Ukvm starts up with a bootstrap GDT which is "invisible" to the guest, init
 * and switch to our own GDT.
 */
void gdt_init(void)
{
    struct gdtptr gdtptr;

    /* initialize GDT "pointer" */
    gdtptr.limit = sizeof(cpu_gdt64) - 1;
    gdtptr.base = (uint64_t)&cpu_gdt64;

    /* clear structures */
    memset(cpu_gdt64, 0, sizeof(cpu_gdt64));

    cpu_gdt64[GDT_DESC_CODE] = GDT_DESC_CODE_VAL;
    cpu_gdt64[GDT_DESC_DATA] = GDT_DESC_DATA_VAL;

    cpu_gdt_load((uint64_t)&gdtptr);
}
