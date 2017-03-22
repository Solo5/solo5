/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of ukvm, a unikernel monitor.
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

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ukvm_cpu_x86_64.h"

void ukvm_x86_setup_pagetables(uint8_t *mem, size_t mem_size)
{
    uint64_t *pml4 = (uint64_t *)(mem + X86_PML4_BASE);
    uint64_t *pdpte = (uint64_t *)(mem + X86_PDPTE_BASE);
    uint64_t *pde = (uint64_t *)(mem + X86_PDE_BASE);
    uint64_t paddr;

    /*
     * For simplicity we currently use 2MB pages and only a single
     * PML4/PDPTE/PDE.  Sanity check that the guest size is a multiple of the
     * page size and will fit in a single PDE (512 entries).
     */
    assert((mem_size & (X86_GUEST_PAGE_SIZE - 1)) == 0);
    assert(mem_size <= (X86_GUEST_PAGE_SIZE * 512));

    memset(pml4, 0, X86_PML4_SIZE);
    memset(pdpte, 0, X86_PDPTE_SIZE);
    memset(pde, 0, X86_PDE_SIZE);

    *pml4 = X86_PDPTE_BASE | (X86_PDPT_P | X86_PDPT_RW);
    *pdpte = X86_PDE_BASE | (X86_PDPT_P | X86_PDPT_RW);
    for (paddr = 0; paddr < mem_size; paddr += X86_GUEST_PAGE_SIZE, pde++)
        *pde = paddr | (X86_PDPT_P | X86_PDPT_RW | X86_PDPT_PS);
}

static struct x86_gdt_desc seg_to_desc(const struct x86_seg *seg)
{
    struct x86_gdt_desc desc = {
        .base_lo = seg->base & 0xffffff,
        .base_hi = (seg->base & 0xff000000) >> 24,
        .limit_lo = seg->limit & 0xffff,
        .limit_hi = (seg->limit & 0xf0000) >> 16,
        .type = seg->type, .s = seg->s, .dpl = seg->dpl, .p = seg->p,
        .avl = seg->avl,. l = seg->l, .db = seg->db, .g = seg->g
    };
    return desc;
}

void ukvm_x86_setup_gdt(uint8_t *mem)
{
    struct x86_gdt_desc *gdt = (struct x86_gdt_desc *)(mem + X86_GDT_BASE);
    struct x86_gdt_desc null = { 0 };

    gdt[X86_GDT_NULL] = null;
    gdt[X86_GDT_CODE] = seg_to_desc(&ukvm_x86_seg_code);
    gdt[X86_GDT_DATA] = seg_to_desc(&ukvm_x86_seg_data);
    gdt[X86_GDT_TSS_LO] = null;
    gdt[X86_GDT_TSS_HI] = null;
}
