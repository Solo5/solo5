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

#include "kernel.h"

/* kernel | free (heap) | stack */

static uint64_t heap_start;
static uint64_t heap_top;
static uint64_t max_addr;


uint64_t mem_max_addr(void) {
    return max_addr;
}


void mem_init(uint64_t size, uint64_t kernel_end) {
    // Let's give 0x500000 Bytes to the stack
    uint64_t stack_start = size - 0x500000;

    assert(size > 0x500000);
    max_addr = stack_start;
    heap_start = (kernel_end + PAGE_SIZE - 1) & PAGE_MASK;
    heap_top = heap_start;
}

/* for malloc */
void *sbrk(intptr_t increment) { 
    uint64_t ret;

    if ( increment == 0 )
        return (void *)heap_top;

    assert(increment >= PAGE_SIZE);
    assert((increment % PAGE_SIZE) == 0);

    ret = heap_top;
    heap_top += increment;
    assert(heap_top < max_addr);

    return (void *)ret; 
}
