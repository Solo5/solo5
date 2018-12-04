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
 * If we want support memory greater than 512GB, we should
 * modify the following PAGE_TABLE_OFFSET at the same time.
 */

/* First page in AARCH64_PAGE_TABLE is used for PGD */
#define PGT_PGD_START   0

/* Second page in AARCH64_PAGE_TABLE is used for PUD */
#define PGT_PUD_START   1

/* Third page in AARCH64_PAGE_TABLE is used for PMD */
#define PGT_PMD_START   2

/* Fourth page in AARCH64_PAGE_TABLE is used for PTE */
#define PGT_PTE_START   3

#define PGT_PAGE_ADDR(x)   (AARCH64_PAGE_TABLE + PAGE_SIZE * (x))

enum PGT_LEVEL {
    L1_TABLE = 1, // PUD
    L2_TABLE = 2, // PMD
    L3_TABLE = 3, // PTE
};

static uint64_t aarch64_create_pagetable(enum PGT_LEVEL level,
                        uint8_t *va_addr, uint64_t start, uint64_t size,
                        uint64_t attrs, uint64_t attrs_io)
{
    uint32_t idx;
    uint64_t *pentry;
    uint64_t phys_table_addr, block_size, out_address;

    if (level == L1_TABLE) {
        /* The block size of each PUD entry is 1GB */
        block_size = PUD_SIZE;
        /* Locate the page of PUD to do this translation */
        phys_table_addr = PGT_PAGE_ADDR(PGT_PUD_START + DIV_ROUND_UP(start, PGD_SIZE));
    } else if (level == L2_TABLE) {
        /* The block size of each PMD entry is 2MB */
        block_size = PMD_SIZE;
        /* Locate the page of PMD to do this translation */
        phys_table_addr = PGT_PAGE_ADDR(PGT_PMD_START + DIV_ROUND_UP(start, PUD_SIZE));
    } else if (level == L3_TABLE) {
        /* The block size of each PTE entry is 4KB */
        block_size = PAGE_SIZE;
        /* Locate the page of PTE to do this translation */
        phys_table_addr = PGT_PAGE_ADDR(PGT_PTE_START + DIV_ROUND_UP(start, PMD_SIZE));
    } else
        err(1, "Unknown page table level: %d\n", level);

    /* Fill table entries */
    pentry = (uint64_t *)(va_addr + phys_table_addr);
    for (idx = 0; size > 0; idx++) {
        out_address = start + block_size * idx;
        if (out_address >= AARCH64_MMIO_BASE)
            pentry[idx] = out_address | attrs_io;
        else
            pentry[idx] = out_address | attrs;

        size -= block_size;
    }

    return phys_table_addr;
}

static void aarch64_unmap_zero_page(uint8_t *va_addr)
{
    uint64_t *pentry, phys_table_addr;

    /* Create PMD table for first 1GB */
    phys_table_addr = aarch64_create_pagetable(L2_TABLE,
                        va_addr, 0, PUD_SIZE,
                        PROT_SECT_NORMAL_EXEC,
                        PROT_SECT_DEVICE_nGnRE);

    /* Locate the pud entry for first 1G */
    pentry = (uint64_t *)(va_addr + PGT_PAGE_ADDR(PGT_PUD_START));

    /* Update first 1GB mapping from block to table */
    *pentry = phys_table_addr | PGT_DESC_TYPE_TABLE;

    /* Create PTE table for first 2MB */
    phys_table_addr = aarch64_create_pagetable(L3_TABLE,
                        va_addr, 0, PMD_SIZE,
                        PROT_PAGE_NORMAL_EXEC,
                        PROT_PAGE_DEVICE_nGnRE);

    /* Locate the pmd entry for first 2MB */
    pentry = (uint64_t *)(va_addr + PGT_PAGE_ADDR(PGT_PMD_START));

    /* Update first 2MB mapping from block to table */
    *pentry = phys_table_addr | PGT_DESC_TYPE_TABLE;

    /* Locate the pte entry for first 4KB */
    pentry = (uint64_t *)(va_addr + PGT_PAGE_ADDR(PGT_PTE_START));

    /* Unmap the first 4KB */
    *pentry = 0;

    return;
}

/*
 * We will do VA = PA mapping in page table. For simplicity, currently
 * we use minimal 2MB block size and 1 PUD table in page table.
 */
void aarch64_setup_memory_mapping(uint8_t *va_addr, uint64_t ram_size,
                              uint64_t phy_space_size)
{
    uint64_t *pentry;
    uint32_t idx;
    uint64_t map_size, pud_table;

    /*
     * In order to keep consistency with x86_64, we limit hvt_hypercall only
     * to support sending 32-bit pointers. So we limit the guest to support
     * only 4GB memory. This will avoid using additional code to guarantee the
     * hypercall parameters are using the memory below 4GB.
     *
     * Address above 4GB is using for MMIO space now. This would be changed
     * easily if the design of hvt_hypercall would be changed in the future.
     */
    if (ram_size > AARCH64_MMIO_BASE)
        err(1, "The guest memory [0x%lx] exceeds the max size [0x%lx]\n",
            ram_size, AARCH64_MMIO_BASE);

    /* Allocate one page for PGD to support > 512GB address space */
    pentry = (uint64_t *)(va_addr + PGT_PAGE_ADDR(PGT_PGD_START));

    /* Mapping whole physical space to virtual space */
    for (idx = 0; phy_space_size > 0; idx++) {
        /* The translation size of each PUD table is 512GB (PGD_SIZE) */
        map_size = (phy_space_size > PGD_SIZE) ? PGD_SIZE : phy_space_size;

        /* Mapping VA <=> PA for RAM */
        pud_table = aarch64_create_pagetable(L1_TABLE, va_addr,
		                            PGD_SIZE * idx, map_size,
                                    PROT_SECT_NORMAL_EXEC, PROT_SECT_DEVICE_nGnRE);

        pentry[idx] = pud_table | PGT_DESC_TYPE_TABLE;
        phy_space_size -= map_size;
    }

    /* Unmap the zero page for NULL */
    aarch64_unmap_zero_page(va_addr);

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
