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

static uint64_t heap_start;
static uint64_t heap_top;
static uint64_t stack_guard_size;

void mem_init(void)
{
    extern char _stext[], _etext[], _erodata[], _end[];
    uint64_t mem_size;

    mem_size = platform_mem_size();
    heap_start = ((uint64_t)&_end + PAGE_SIZE - 1) & PAGE_MASK;
    heap_top = heap_start;
    /*
     * Cowardly refuse to run with less than 512KB of free memory.
     */
    if (heap_start + 0x80000 > mem_size)
	PANIC("Not enough memory");
    /*
     * If we have <1MB of free memory then don't let the heap grow to more than
     * roughly half of free memory, otherwise don't let it grow to within 1MB
     * of the stack.
     * TODO: Use guard pages here instead?
     */
    stack_guard_size = (mem_size - heap_start >= 0x100000) ?
	0x100000 : ((mem_size - heap_start) / 2);

    log(INFO, "Solo5: Memory map: %lu MB addressable:\n", mem_size >> 20);
    log(INFO, "Solo5:     unused @ (0x0 - 0x%lx)\n", &_stext[-1]);
    log(INFO, "Solo5:       text @ (0x%lx - 0x%lx)\n", &_stext, &_etext[-1]);
    log(INFO, "Solo5:     rodata @ (0x%lx - 0x%lx)\n", &_etext, &_erodata[-1]);
    log(INFO, "Solo5:       data @ (0x%lx - 0x%lx)\n", &_erodata, &_end[-1]);
    log(INFO, "Solo5:       heap >= 0x%lx < stack < 0x%lx\n", heap_start,
        mem_size);
}

/* 
 * Allocate a 4K aligned chunk on the brk.  Should only be called
 * before solo5_app_main. 
 */
void *alloc_chunk_4K(size_t size)
{
    uint64_t prev, brk;
    uint64_t heap_max = (uint64_t)&prev - stack_guard_size;
    prev = brk = heap_top;

    /* for 4K alignment */
    prev = brk = brk + (0x1000 - (brk & 0xfff));

    brk += size;
    if (brk >= heap_max || brk < heap_start)
        return NULL;

    heap_top = brk;
    return (void *)prev;
}

void solo5_get_info(struct solo5_info *info) {
    info->heap_start = heap_top;
    info->heap_end = platform_mem_size() - stack_guard_size;
}
