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
static uint64_t max_addr;

uint64_t mem_max_addr(void)
{
    return max_addr;
}

void mem_init(uint64_t size, uint64_t kernel_end)
{
    extern char _stext[], _etext[], _erodata[], _end[];

    max_addr = size;
    heap_start = (kernel_end + PAGE_SIZE - 1) & PAGE_MASK;
    heap_top = heap_start;

    printf("Solo5: Memory map: %lu MB addressable:\n", max_addr >> 20);
    printf("Solo5:     unused @ (0x0 - 0x%lx)\n", &_stext[-1]);
    printf("Solo5:       text @ (0x%lx - 0x%lx)\n", &_stext, &_etext[-1]);
    printf("Solo5:     rodata @ (0x%lx - 0x%lx)\n", &_etext, &_erodata[-1]);
    printf("Solo5:       data @ (0x%lx - 0x%lx)\n", &_erodata, &_end[-1]);
    printf("Solo5:       heap >= 0x%lx < stack < 0x%lx\n", heap_start,
        max_addr);
}

/*
 * Prevent the heap from overflowing into the stack.
 * TODO: Use guard pages here, this does not protect from the converse.
 */
#define STACK_GUARD_SIZE 0x100000

/*
 * Called by dlmalloc to allocate or free memory.
 */
void *sbrk(ptrdiff_t increment)
{
    uint64_t prev, brk;
    uint64_t heap_max = (uint64_t)&prev - STACK_GUARD_SIZE;
    prev = brk = heap_top;

    /*
     * dlmalloc guarantees increment values less than half of size_t, so this
     * is safe from overflow.
     */
    brk += increment;
    if (brk >= heap_max || brk < heap_start)
        return (void *)-1;

    heap_top = brk;
    return (void *)prev;
}
