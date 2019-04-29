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

static uint64_t heap_start;

/*
 * Locks the memory layout (by disabling mem_ialloc_pages()). Must be called
 * before passing control to the application via solo5_app_main().
 *
 * Returns the first usable memory address for application heap in (*start)
 * and the size of the heap in (*size).
 */
static int mem_locked = 0;
void mem_lock_heap(uintptr_t *start, size_t *size)
{
    assert(!mem_locked);

    mem_locked = 1;
    *start = heap_start;
    *size = platform_mem_size() - heap_start;
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
    log(INFO, "Solo5:       heap >= 0x%llx < stack < 0x%llx\n",
            (unsigned long long)heap_start, (unsigned long long)mem_size);
}

/*
 * Allocate pages on the heap.  Should only be called on
 * initialization (before solo5_app_main).
 */
void *mem_ialloc_pages(size_t num)
{
    assert(!mem_locked);

    uint64_t prev = heap_start;
    heap_start += num << PAGE_SHIFT;
    assert(heap_start < (uint64_t)&prev);

    return (void *)prev;
}
