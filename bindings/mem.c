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

#include "bindings.h"

static uint64_t heap_start;
static const size_t stack_size = 4 * PAGE_SIZE;

static uint64_t heap_end(void) {
    return platform_mem_size() - stack_size;
}

void mem_init(void)
{
    extern char _stext[], _etext[], _erodata[], _end[];
    uint64_t mem_size;

    mem_size = platform_mem_size();
    heap_start = ((uint64_t)&_end + PAGE_SIZE - 1) & PAGE_MASK;

    /*
     * Cowardly refuse to run with less than 512KB of free memory.
     */
    if (heap_start + 0x80000 > mem_size)
	PANIC("Not enough memory", NULL);

    log(INFO, "Solo5: Memory map: %llu MB addressable:\n",
            (unsigned long long)mem_size >> 20);
    log(INFO, "Solo5:   reserved @ (0x0 - 0x%llx)\n",
            (unsigned long long)_stext-1);
    log(INFO, "Solo5:       text @ (0x%llx - 0x%llx)\n",
            (unsigned long long)_stext, (unsigned long long)_etext-1);
    log(INFO, "Solo5:     rodata @ (0x%llx - 0x%llx)\n",
            (unsigned long long)_etext, (unsigned long long)_erodata-1);
    log(INFO, "Solo5:       data @ (0x%llx - 0x%llx)\n",
            (unsigned long long)_erodata, (unsigned long long)_end-1);
    log(INFO, "Solo5:       heap @ (0x%llx - 0x%llx)\n",
            (unsigned long long)heap_start, (unsigned long long)heap_end());
    log(INFO, "Solo5:      stack @ (0x%llx - 0x%llx)\n",
            (unsigned long long)(mem_size - stack_size), (unsigned long long)mem_size);
}

void mem_info(struct solo5_start_info* si) {
    si->stack_start = (uintptr_t)(platform_mem_size() - stack_size);
    si->stack_size = stack_size;
    si->mem_page = PAGE_SIZE;
    si->mem_avail = (heap_end() - heap_start) / PAGE_SIZE;
}

static int mem_locked = 0;

void *solo5_mem_alloc(size_t num)
{
    assert(!mem_locked);
    assert(heap_start < heap_end());

    uint64_t prev = heap_start;
    heap_start += num << PAGE_SHIFT;
    assert(heap_start < (uint64_t)&prev);

    return (void *)prev;
}

void solo5_mem_lock(void)
{
    assert(!mem_locked);
    mem_locked = 1;
}
