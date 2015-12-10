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

#include "loader.h"

/* All page table levels have 512 64-bit entries */
#define NUM_ENTRIES 512
#define ENTRY_PRESENT 1
#define ENTRY_RW      2

uint32_t page_table_area;
uint32_t page_bitmap_area;

/* in to64.s */
void to64_prep_paging(uint32_t pml4);

#define NUM_PTES(m) ((((m) - 1)/ PAGE_SIZE) + 1)
#define NUM_PTPGS(m) (((NUM_PTES(m) - 1)/ NUM_ENTRIES) + 1)
#define NUM_PDPGS(m) (((NUM_PTPGS(m) - 1)/ NUM_ENTRIES) + 1)
#define NUM_PDPPGS(m) (((NUM_PDPGS(m) - 1)/ NUM_ENTRIES) + 1)
#define NUM_PML4PGS(m) (((NUM_PDPPGS(m) - 1)/ NUM_ENTRIES) + 1)

#define PT_NUM_PAGES(m) (NUM_PTPGS(m)           \
                         + NUM_PDPGS(m)         \
                         + NUM_PDPPGS(m)        \
                         + NUM_PML4PGS(m))

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

#undef assert
#define assert(e) do {                                       \
        if (!(e)) {                                          \
            printk("LOADER: assertion failed: \"%s\"", #e);  \
            for(;;);                                         \
        }                                                    \
    } while(0)

#define PT_ADDR(x) ((uint32_t)(x) & ~0xfff)
static void walk_pagetable(uint64_t max_addr) {
    uint32_t pml4 = PML4(0);
    int dbg = 0;

    if (dbg) {
        printk("pml4pgs: %d\n", NUM_PML4PGS(max_addr));
        printk("pdppgs:  %d\n", NUM_PDPPGS(max_addr));
        printk("pdpgs:   %d\n", NUM_PDPGS(max_addr));
        printk("ptpgs:   %d\n", NUM_PTPGS(max_addr));
        
        printk("pml4(0) is at 0x%lx\n", PML4(0));
        printk("pml4e(0) is 0x%llx ", PT_ADDR(*(uint64_t *)pml4));
        printk("should be 0x%llx\n", PML4E(0));
    }
    assert(PT_ADDR(*(uint64_t *)pml4) == PML4E(0));

    uint64_t *pdp, *pd, *pt;
    size_t i, j, k;
    for (i = 0; i < NUM_ENTRIES; i++) {
        pdp = (uint64_t *)(PT_ADDR(*(uint64_t *)pml4)) + i;
        if (!(*pdp & 0x1))
            continue;

        if (dbg) {
            printk("PDP(%d) is at ", i);
            printk("0x%llx ", pdp);
            printk("should be 0x%llx\n", PDP(i));
        }
        assert((uint32_t)pdp == PDP(i));
        if (dbg) {
            printk("PDPE(%d) is at ", i);
            printk("0x%llx ", PT_ADDR(*pdp));
            printk("should be 0x%llx\n", PDPE(i));
        }
        assert(PT_ADDR(*pdp) == PDPE(i));

        for (j = 0; j < NUM_ENTRIES; j++) {
            pd = ((uint64_t *)PT_ADDR(*pdp)) + j; 
            if (!(*pd & 0x1))
                continue;
            size_t jdx = i * NUM_ENTRIES + j;
            if (dbg) {
                printk("PD(%d) is at ", jdx);
                printk("0x%llx ", pd);
                printk("should be 0x%llx\n", PD(jdx));
            }
            assert((uint32_t)pd == PD(jdx));
            if (dbg) {
                printk("PDE(%d) is at ", jdx);
                printk("0x%llx ", PT_ADDR(*pd));
                printk("should be 0x%llx\n", PDE(jdx));
            }
            assert(PT_ADDR(*pd) == PDE(jdx));

            for (k = 0; k < NUM_ENTRIES; k++) {
                pt = ((uint64_t *)PT_ADDR(*pd)) + k; 
                if (!(*pt & 0x1))
                    continue;
                size_t idx = jdx * NUM_ENTRIES + k;
                if (dbg) {
                    printk("PT(%d) is at ", idx);
                    printk("0x%llx ", pt);
                    printk("should be 0x%llx\n", PT(idx));
                }
                assert((uint32_t)pt == PT(idx));
                if (dbg) {
                    printk("PTE(%d) is at ", idx);
                    printk("0x%llx ", PT_ADDR(*pt));
                    printk("should be 0x%llx\n", PTE(idx));
                }
                assert(PT_ADDR(*pt) == PTE(idx));
            }
        }
    }
}

void pagetable_init(uint64_t max_addr, uint32_t kernel_end) {
    uint32_t i;

    page_table_area = kernel_end;
    memset((uint8_t *)page_table_area, 0, PT_NUM_PAGES(max_addr) * PAGE_SIZE);
    printk("page_table_area: 0x%lx\n", page_table_area);
    printk("page_table_end:  0x%lx\n", page_table_area + PT_NUM_PAGES(max_addr) * PAGE_SIZE);

    /* direct map all but the zero page in the page tables */
    for (i = 1; i < NUM_PTES(max_addr); i++ ) {
        struct dw *pt = (struct dw *)PT(i);
        pt->lo = PTE(i) | ENTRY_RW | ENTRY_PRESENT;
    }

    /* set up the page directories */
    for (i = 0; i < NUM_PTPGS(max_addr); i++) {
        struct dw *pd = (struct dw *)PD(i);
        pd->lo = PDE(i) | ENTRY_RW | ENTRY_PRESENT;
    }

    /* set up the pdp's  */
    for (i = 0; i < NUM_PDPGS(max_addr); i++) {
        struct dw *pdp = (struct dw *)PDP(i);
        pdp->lo = PDPE(i) | ENTRY_RW | ENTRY_PRESENT;
    }

    /* set up the pml4  */
    for (i = 0; i < NUM_PDPPGS(max_addr); i++) {
        struct dw *pml4 = (struct dw *)PML4(i);
        pml4->lo = PML4E(i) | ENTRY_RW | ENTRY_PRESENT;
    }

    walk_pagetable(max_addr);

    to64_prep_paging(PML4(0));
    //return PML4(0);
}

