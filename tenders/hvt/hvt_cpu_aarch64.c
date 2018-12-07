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

/*
 * hvt_cpu_aarch64.c: Common architecture-dependent code supporting aarch64
 * backend implementations.
 */
#include <err.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "hvt_cpu_aarch64.h"

/*
 * We will do VA = PA mapping in page table. For simplicity, currently
 * we use minimal 2MB block size and 1 PUD table in page table.
 */
void aarch64_setup_memory_mapping(uint8_t *mem, uint64_t mem_size)
{
    uint64_t paddr;
    uint64_t *pgd = (uint64_t *)(mem + AARCH64_PGD_PGT_BASE);
    uint64_t *pud = (uint64_t *)(mem + AARCH64_PUD_PGT_BASE);
    uint64_t *pmd = (uint64_t *)(mem + AARCH64_PMD_PGT_BASE);
    uint64_t *pte = (uint64_t *)(mem + AARCH64_PTE_PGT_BASE);

    /*
     * In order to keep consistency with x86_64, we limit hvt_hypercall only
     * to support sending 32-bit pointers. So we limit the guest to support
     * only 4GB memory. This will avoid using additional code to guarantee the
     * hypercall parameters are using the memory below 4GB.
     *
     * Address above 4GB is using for MMIO space now. This would be changed
     * easily if the design of hvt_hypercall would be changed in the future.
     */
    if (mem_size > AARCH64_MMIO_BASE) {
        err(1, "The guest memory [0x%lx] exceeds the max size [0x%lx]\n",
            mem_size, AARCH64_MMIO_BASE);
    } else if (mem_size < AARCH64_GUEST_BLOCK_SIZE) {
        err(1, "The guest memory [0x%lx] less than [0x%lx]\n",
            mem_size, AARCH64_GUEST_BLOCK_SIZE);
    }

    /* Zero all page tables */
    memset(pgd, 0, AARCH64_PGD_PGT_SIZE);
    memset(pud, 0, AARCH64_PUD_PGT_SIZE);
    memset(pmd, 0, AARCH64_PMD_PGT_SIZE);
    memset(pte, 0, AARCH64_PTE_PGT_SIZE);

    /* Map first 2MB block in pte table */
    for (paddr = 0; paddr < AARCH64_GUEST_BLOCK_SIZE;
         paddr += PAGE_SIZE, pte++) {
        /*
         * Leave all pages below AARCH64_PGT_MAP_START unmapped in the guest.
         * This includes the zero page and the guest's page tables.
         */
        if (paddr < AARCH64_PGT_MAP_START)
            continue;

        /*
         * Map the remainder of the pages below AARCH64_GUEST_MIN_BASE
         * as read-only; these are used for input from hvt to the guest
         * only, with the rest reserved for future use.
         */
        if (paddr < AARCH64_GUEST_MIN_BASE)
            *pte = paddr | PROT_PAGE_NORMAL_RO;
        else
            *pte = paddr | PROT_PAGE_NORMAL_EXEC;
    }
    assert(paddr == AARCH64_GUEST_BLOCK_SIZE);

    /* Link pte table to pmd[0] */
    *pmd++ = AARCH64_PTE_PGT_BASE | PGT_DESC_TYPE_TABLE;

    /* Mapping left memory by 2MB block in pmd table */
    for (; paddr < mem_size; paddr += PMD_SIZE, pmd++)
        *pmd = paddr | PROT_SECT_NORMAL_EXEC;

    /* Link pmd tables to pud[0..3] */
    *pud++ = AARCH64_PMD_PGT_BASE | PGT_DESC_TYPE_TABLE;
    *pud++ = (AARCH64_PMD_PGT_BASE + 0x1000) | PGT_DESC_TYPE_TABLE;
    *pud++ = (AARCH64_PMD_PGT_BASE + 0x2000) | PGT_DESC_TYPE_TABLE;
    *pud++ = (AARCH64_PMD_PGT_BASE + 0x3000) | PGT_DESC_TYPE_TABLE;

    /* Mapping MMIO */
    for (paddr = AARCH64_MMIO_BASE;
         paddr < AARCH64_MMIO_BASE + AARCH64_MMIO_SZ;
         paddr += PUD_SIZE, pud++)
        *pud = paddr | PROT_SECT_DEVICE_nGnRE;

    /* Link pud table to pgd[0] */
    *pgd = AARCH64_PUD_PGT_BASE | PGT_DESC_TYPE_TABLE;
}

void aarch64_mem_size(size_t *mem_size) {
    size_t mem;
    mem = (*mem_size / PMD_SIZE) * PMD_SIZE;
    assert (mem <= *mem_size);
    if (mem < *mem_size)
        warnx("adjusting memory to %zu bytes", mem);
    if (mem > AARCH64_MMIO_BASE)
        err(1, "guest memory size %zu bytes exceeds the max size %lu bytes",
            mem, AARCH64_MMIO_BASE);
    *mem_size = mem;
}
