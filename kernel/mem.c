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

/* free | kernel | page_table_area | page_bitmap_area | free | stack */

static uint64_t kernel_start;
static uint64_t kernel_end;
static uint64_t page_table_area;
static uint64_t page_bitmap_area;
static uint64_t heap_start;
static uint64_t heap_top;

static uint64_t max_addr;

#define INITIAL_STACK_PAGES 4
#define NUM_ENTRIES 512

#define PT_NUM_PTES(m) ((((m) - 1)/ PAGE_SIZE) + 1)
#define PT_NUM_PTPGS(m) (((PT_NUM_PTES(m) - 1)/ NUM_ENTRIES) + 1)
#define PT_NUM_PDPGS(m) (((PT_NUM_PTPGS(m) - 1)/ NUM_ENTRIES) + 1)
#define PT_NUM_PDPPGS(m) (((PT_NUM_PDPGS(m) - 1)/ NUM_ENTRIES) + 1)
#define PT_NUM_PML4PGS(m) (((PT_NUM_PDPPGS(m) - 1)/ NUM_ENTRIES) + 1)

#define PT_NUM_PAGES(m) (PT_NUM_PTPGS(m) \
                         + PT_NUM_PDPGS(m) \
                         + PT_NUM_PDPPGS(m) \
                         + PT_NUM_PML4PGS(m))

/* PTE(x) should contain the address of page x */
/* PTE(x) should be stored at PT(x) */
#define PTE(x)  ((x) * PAGE_SIZE)
#define PT(x)   (page_table_area                \
                 + ((x) * sizeof(uint64_t)))

/* PDE(x) should contain PT(x * NUM_ENTRIES) */
/* PDE(x) should be stored at PD(x) */
#define PDE(x)  (PT((x) * NUM_ENTRIES))
#define PD(x)   (page_table_area                                \
                 + (uint32_t)(NUM_PTPGS(max_addr) * PAGE_SIZE)  \
                 + ((x) * sizeof(uint64_t)))

/* PDPE(x) should contain PD(x * NUM_ENTRIES) */
/* PDPE(x) should be stored at PDP(x) */
#define PDPE(x) (PD((x) * NUM_ENTRIES))
#define PDP(x)  (page_table_area                                    \
                 +  (uint32_t)((NUM_PTPGS(max_addr)                 \
                                + NUM_PDPGS(max_addr)) * PAGE_SIZE) \
                 + ((x) * sizeof(uint64_t)))

/* PML4E(x) should contain PDP(x * NUM_ENTRIES) */
/* PML4E(x) should be stored at PML4(x) */
#define PML4E(x) (PDP((x) * NUM_ENTRIES))
#define PML4(x) (page_table_area                                        \
                 +  (uint32_t)((NUM_PTPGS(max_addr)                     \
                                + NUM_PDPGS(max_addr)                   \
                                + NUM_PDPPGS(max_addr)) * PAGE_SIZE)    \
                 + ((x) * sizeof(uint64_t)))

#define PGBITS_SIZE(m) ((((m) >> PAGE_SHIFT) + 1) / 8)
#define PGBITS_BYTE(a) ((page_bitmap_area + ((a) >> PAGE_SHIFT) / 8))
#define PGBITS_MASK(a) (1 << (((a) >> PAGE_SHIFT) & 0x7))
#define PGBITS_SET(a)                                                   \
    *(uint8_t *)PGBITS_BYTE(a) = (*(uint8_t *)PGBITS_BYTE(a)) | PGBITS_MASK(a);
#define PGBITS_CLEAR(a) do {                            \
        uint8_t *byte = (uint8_t *)PGBITS_BYTE(a);      \
        uint8_t tmp = *byte & ~PGBITS_MASK(a);          \
        *byte = tmp;                                    \
    } while (0)
#define PGBITS_READ(a)                                      \
    (((*(uint8_t *)PGBITS_BYTE(a)) & PGBITS_MASK(a)) != 0)



static void page_bitmap_init(uint32_t mmap, uint32_t mmap_length) {
    size_t i, j;

    page_bitmap_area = page_table_area + (PT_NUM_PAGES(max_addr) * PAGE_SIZE);

    /* mark everything as unavailable to start with */
    memset((uint8_t *)page_bitmap_area, 0xff, PGBITS_SIZE(max_addr));

    for (i = 0; i < mmap_length; i += sizeof(multiboot_memory_map_t)) {
        multiboot_memory_map_t *m = (multiboot_memory_map_t *)(mmap + i);

        /* leave as unavailable if it's not in the memory map */
        if (m->type != MULTIBOOT_MEMORY_AVAILABLE) 
            continue;

        for (j = 0; j < m->len; j += PAGE_SIZE) {
            /* don't mark the 0 page as available */
            if (m->addr + j == 0)
                continue;

            /* mark as available unless it's the kernel + pt + pt_bitmap area */
            if ((m->addr + j < kernel_start) 
                || (m->addr + j >= page_bitmap_area + PGBITS_SIZE(max_addr))) {
                
                assert(PGBITS_READ(m->addr + j));
                PGBITS_CLEAR(m->addr + j);
                assert(!PGBITS_READ(m->addr + j));
            }
        }
    }
}

static void allocate_page(uint64_t addr) {
    if ( PGBITS_READ(addr) )
        PANIC("addr 0x%llx is already allocated\n", addr);
    assert(!PGBITS_READ(addr));
    PGBITS_SET(addr);
}

static void allocate_stack(uint64_t stack_top) {
    int i;

    for (i = 0; i < INITIAL_STACK_PAGES; i++)
        allocate_page(stack_top - (i * PAGE_SIZE));

    /* XXX unmap page at bottom of stack to protect from overrun */
}

static void clear_bss(void) {
    extern uint8_t bss_end;
    extern uint8_t bss_start;
    size_t bss_size = (&bss_end) - (&bss_start);
    memset(&bss_start, 0, bss_size);
}

uint64_t mem_max_addr(void) {
    return max_addr;
}

void mem_init(struct multiboot_info *mb) {
    uint64_t max_avail = 0;
    multiboot_module_t *kernel;
    size_t i;

    /* the loader should really do this for us */
    clear_bss();

    for (i = 0; i < mb->mmap_length; i += sizeof(multiboot_memory_map_t)) {
        multiboot_memory_map_t *m;
        m = (multiboot_memory_map_t *)(mb->mmap_addr + i);
        max_addr = m->addr + m->len;
        if ( m->type == MULTIBOOT_MEMORY_AVAILABLE )
            max_avail = max_addr;
    }

    kernel = (multiboot_module_t *)((uint64_t)mb->mods_addr);


    kernel_start = kernel->mod_start;
    kernel_end = kernel->mod_end;
    page_table_area = (kernel_end & PAGE_MASK) + PAGE_SIZE;

    page_bitmap_init(mb->mmap_addr, mb->mmap_length);
    heap_start = (page_bitmap_area + PGBITS_SIZE(max_addr) + PAGE_SIZE - 1) 
        & PAGE_MASK;
    heap_top = heap_start;

    allocate_stack(max_avail - 8);
}

/* for malloc */
void *sbrk(intptr_t increment) { 
    uint64_t ret;
    int i;

    if ( increment == 0 )
        return (void *)heap_top;

    assert(increment >= PAGE_SIZE);
    assert((increment % PAGE_SIZE) == 0);

    for ( i = 0; i < increment; i += PAGE_SIZE )
        allocate_page(heap_top + i);

    ret = heap_top;
    heap_top += increment;

    return (void *)ret; 
}
