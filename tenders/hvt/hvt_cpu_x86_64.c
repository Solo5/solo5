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

/*
 * hvt_cpu_x86_64.c: Common architecture-dependent code supporting x86_64
 * backend implementations.
 */

#include <err.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hvt_cpu_x86_64.h"

void hvt_x86_mem_size(size_t *mem_size) {
    size_t mem;
    mem = (*mem_size / X86_GUEST_PAGE_SIZE) * X86_GUEST_PAGE_SIZE;
    assert (mem <= *mem_size);
    if (mem == 0)
        mem = X86_GUEST_PAGE_SIZE;
    if (mem != *mem_size)
        warnx("adjusting memory to %zu bytes", mem);
    if (mem > X86_GUEST_MAX_MEM_SIZE)
        errx(1, "guest memory size %zu bytes exceeds the max size %zu bytes",
            mem, X86_GUEST_MAX_MEM_SIZE);
    *mem_size = mem;
}

void hvt_x86_setup_pagetables(uint8_t *mem, size_t mem_size)
{
    uint64_t *pml4 = (uint64_t *)(mem + X86_PML4_BASE);
    uint64_t *pdpte = (uint64_t *)(mem + X86_PDPTE_BASE);
    uint64_t *pde = (uint64_t *)(mem + X86_PDE_BASE);
    uint64_t *pt0e = (uint64_t *)(mem + X86_PT0E_BASE);
    uint64_t paddr;

    /*
     * We currently use 2MB pages.  Sanity check that the guest size is a
     * multiple of the page size and will fit in up to four PDEs (512 entries /
     * 4GB). Additionally, check that the guest size is at least 2MB.
     */
    assert((mem_size & (X86_GUEST_PAGE_SIZE - 1)) == 0);
    assert(mem_size <= X86_GUEST_MAX_MEM_SIZE);
    assert(mem_size >= X86_GUEST_PAGE_SIZE);

    memset(pml4, 0, X86_PML4_SIZE);
    memset(pdpte, 0, X86_PDPTE_SIZE);
    memset(pde, 0, X86_PDE_SIZE);
    memset(pt0e, 0, X86_PTE_SIZE);

    *pml4 = X86_PDPTE_BASE | (X86_PDPT_P | X86_PDPT_RW);
    *pdpte = X86_PDE_BASE | (X86_PDPT_P | X86_PDPT_RW);
    pdpte++;
    *pdpte = (X86_PDE_BASE + 0x1000) | (X86_PDPT_P | X86_PDPT_RW);
    pdpte++;
    *pdpte = (X86_PDE_BASE + 0x2000) | (X86_PDPT_P | X86_PDPT_RW);
    pdpte++;
    *pdpte = (X86_PDE_BASE + 0x3000) | (X86_PDPT_P | X86_PDPT_RW);

    /*
     * PDE[0] is special: use 4kB pages and PT0 for the first 2MB of address
     * space.
     */
    *pde = X86_PT0E_BASE | (X86_PDPT_P | X86_PDPT_RW);
    pde++;
    for (paddr = 0; paddr < X86_GUEST_PAGE_SIZE; paddr += 0x1000, pt0e++) {
        /*
         * Leave all pages below X86_PT0_MAP_START not-present in the guest.
         *
         * This includes the zero page, boot GDT and the guest's page tables
         * themselves.
         */
        if (paddr < X86_PT0_MAP_START)
            continue;
	/*
	 * Map the remainder of the pages below X86_GUEST_MIN_BASE as read-only;
	 * these are used for input from hvt to the guest only, with the rest
	 * reserved for future use.
	 */
        if (paddr < X86_GUEST_MIN_BASE)
            *pt0e = paddr | X86_PDPT_P;
	else
            *pt0e = paddr | (X86_PDPT_P | X86_PDPT_RW);
    }
    assert(paddr == X86_GUEST_PAGE_SIZE);
    for (; paddr < mem_size; paddr += X86_GUEST_PAGE_SIZE, pde++)
        *pde = paddr | (X86_PDPT_P | X86_PDPT_RW | X86_PDPT_PS);
}

static struct x86_gdt_desc sreg_to_desc(const struct x86_sreg *sreg)
{
    /*
     * Translate our shadow register representation to a GDT entry.
     *
     * XXX: This function will only work correctly for certain descriptor types
     * add assertions/tests to verify inputs.
     *
     * NOTE: Technically to boot the VCPU we only need the shadow registers
     * set, everything will work fine with no GDT until the guest attempts to
     * load a segment register.
     */
    uint32_t limit = (sreg->g) ? ((sreg->limit & 0xfffff000) >> 12)
                               : sreg->limit;
    struct x86_gdt_desc desc = {
        .base_lo = sreg->base & 0xffffff,
        .base_hi = (sreg->base & 0xff000000) >> 24,
        .limit_lo = limit & 0xffff,
        .limit_hi = (limit & 0xf0000) >> 16,
        .type = sreg->type, .s = sreg->s, .dpl = sreg->dpl, .p = sreg->p,
        .avl = sreg->avl, .l = sreg->l, .db = sreg->db, .g = sreg->g
    };
    return desc;
}

void hvt_x86_setup_gdt(uint8_t *mem)
{
    struct x86_gdt_desc *gdt = (struct x86_gdt_desc *)(mem + X86_GDT_BASE);
    struct x86_gdt_desc null = { 0 };

    gdt[X86_GDT_NULL] = null;
    gdt[X86_GDT_CODE] = sreg_to_desc(&hvt_x86_sreg_code);
    gdt[X86_GDT_DATA] = sreg_to_desc(&hvt_x86_sreg_data);
}
