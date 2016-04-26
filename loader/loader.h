/* Copyright (c) 2015, IBM 
 * Author(s): Dan Williams <djwillia@us.ibm.com> 
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

#ifndef __LOADER_H__
#define __LOADER_H__
#include <stddef.h>
#include <stdint.h>

#include "multiboot.h"
#include "loader_info.h"

#if !defined(__i386__)
#error "Loader should be 32-bit!"
#endif

#undef LOADER_DEBUG
#ifdef LOADER_DEBUG
#include "../kernel/kernel.h"
#else 
#define serial_init() do {} while (0)
#define printk(x...) do {} while (0)
#endif

struct __attribute__((__packed__)) dw {
    uint32_t lo;
    uint32_t hi;
};

/* functions in page_table.c */
void pagetable_init(uint64_t max_addr, uint32_t kernel_end);

/* functions in gdt.c */
void gdt_init(void);

/* functions in to64.s */
void to64_jump(uint32_t entry_point, uint32_t arg, uint64_t max_addr);

/* functions in lib.c */
void *memset(void *ptr, uint8_t c, size_t size);
#endif
