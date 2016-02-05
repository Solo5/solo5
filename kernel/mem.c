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
