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
 * hvt_cpu_aarch64.h: CPU constants and initialisation data common to aarch64
 * backend implementations.
 */

#ifndef HVT_CPU_AARCH64_H
#define HVT_CPU_AARCH64_H

#include "hvt_abi.h"

#ifndef _BITUL

#ifdef __ASSEMBLY__
#define _AC(X,Y)                X
#define _AT(T,X)                X
#else
#define __AC(X,Y)               (X##Y)
#define _AC(X,Y)                __AC(X,Y)
#define _AT(T,X)                ((T)(X))
#endif

#define _BITUL(x)               (_AC(1,UL) << (x))
#define _BITULL(x)              (_AC(1,ULL) << (x))

#endif

/*
 * As the design of hvt_do_hypercall, the guest memory has been limited
 * to the area of 0 ~ 4GB. So we can design an address space layout for
 * guest as follow:
 *
 * Guest address space layout
 * ---------------------------------------------------------------
 * | 0 ~ 0xFFFFFFFF for RAM | 0x100000000 ~ 0x13FFFFFFF for MMIO |
 * ---------------------------------------------------------------
 * 0x100000000 MMIO space start
 * 0x0FFFFFFFF End of RAM space
 * 0x100000    loaded elf file (linker script dictates location)
 *   ...       unused ram
 * 0x010000    hvt_boot_info starts
 * 0x007000    PTE
 * 0x006000    PMD3
 * 0x005000    PMD2
 * 0x004000    PMD1
 * 0x003000    PMD0, 4 pages to cover max 4GB RAM
 * 0x002000    PUD
 * 0x001000    PGD, memory start for page table
 * 0x000000    unused ram
 */
#define AARCH64_PGD_PGT_BASE     _AC(0x1000, UL)
#define AARCH64_PGD_PGT_SIZE     _AC(0x1000, UL)
#define AARCH64_PUD_PGT_BASE     _AC(0x2000, UL)
#define AARCH64_PUD_PGT_SIZE     _AC(0x1000, UL)
#define AARCH64_PMD_PGT_BASE     _AC(0x3000, UL)
#define AARCH64_PMD_PGT_SIZE     _AC(0x4000, UL)
#define AARCH64_PTE_PGT_BASE     _AC(0x7000, UL)
#define AARCH64_PTE_PGT_SIZE     _AC(0x1000, UL)
#define AARCH64_BOOT_INFO        _AC(0x10000, UL)
#define AARCH64_GUEST_MIN_BASE   _AC(HVT_GUEST_MIN_BASE, UL)
#define AARCH64_MMIO_BASE        _AC(0x100000000, UL)
#define AARCH64_MMIO_SZ          _AC(0x40000000, UL)
#define AARCH64_GUEST_BLOCK_SIZE _AC(0x200000, UL)
#define AARCH64_PGT_MAP_START	 AARCH64_BOOT_INFO

#define GENMASK32(h, l) \
    (((~0U) << (l)) & (~0U >> (31 - (h))))
#define GENMASK64(h, l) \
    (((~0UL) << (l)) & (~0UL >> (63 - (h))))

/* Definitions of Page tables */
#define PAGE_SHIFT  12
#define PAGE_SIZE   (1 << (PAGE_SHIFT))

/*
 * Hardware page table definitions.
 *
 * Descriptor type for (PGD, PUD and PMD).
 */
#define PGT_DESC_TYPE_TABLE (_AC(3, UL) << 0)
#define PGT_DESC_TYPE_SECT  (_AC(1, UL) << 0)
#define PGT_DESC_TYPE_PAGE  (_AC(3, UL) << 0)

/*
 * Bit definition for section type descriptor
 */
#define SECT_VALID      (_AC(1, UL) << 0)
#define SECT_USER       (_AC(1, UL) << 6)     /* AP[1] */
#define SECT_RDONLY     (_AC(1, UL) << 7)     /* AP[2] */
#define SECT_S          (_AC(3, UL) << 8)
#define SECT_AF         (_AC(1, UL) << 10)
#define SECT_NG         (_AC(1, UL) << 11)
#define SECT_CONT       (_AC(1, UL) << 52)
#define SECT_PXN        (_AC(1, UL) << 53)
#define SECT_UXN        (_AC(1, UL) << 54)

/*
 * AttrIndx[2:0] encoding (mapping attributes defined in the MAIR* registers).
 */
#define ATTRINDX(t)     (_AC(t, UL) << 2)

/* Memory types available. */
#define MT_DEVICE_nGnRnE    0
#define MT_DEVICE_nGnRE     1
#define MT_DEVICE_GRE       2
#define MT_NORMAL_NC        3
#define MT_NORMAL           4
#define MT_NORMAL_WT        5

#define PROT_SECT_DEFAULT       	(PGT_DESC_TYPE_SECT | SECT_AF | SECT_S)
#define PROT_SECT_NORMAL        	(PROT_SECT_DEFAULT | SECT_PXN | SECT_UXN | ATTRINDX(MT_NORMAL))
#define PROT_SECT_NORMAL_EXEC   	(PROT_SECT_DEFAULT | SECT_UXN | ATTRINDX(MT_NORMAL))
#define PROT_SECT_DEVICE_nGnRE  	(PROT_SECT_DEFAULT | SECT_PXN | SECT_UXN | ATTRINDX(MT_DEVICE_nGnRE))

#define PROT_PAGE_DEFAULT       	(PGT_DESC_TYPE_PAGE | SECT_AF | SECT_S)
#define PROT_PAGE_DEFAULT_NORMAL  	(PROT_PAGE_DEFAULT | ATTRINDX(MT_NORMAL))
#define PROT_PAGE_DEFAULT_DEVICE   	(PROT_PAGE_DEFAULT | ATTRINDX(MT_DEVICE_nGnRE))
#define PROT_PAGE_NORMAL    		(PROT_PAGE_DEFAULT_NORMAL | SECT_PXN | SECT_UXN)
#define PROT_PAGE_NORMAL_RO    		(PROT_PAGE_DEFAULT_NORMAL | SECT_PXN | SECT_UXN | SECT_RDONLY)
#define PROT_PAGE_NORMAL_EXEC   	(PROT_PAGE_DEFAULT_NORMAL | SECT_UXN)
#define PROT_PAGE_NORMAL_EXEC_RO    (PROT_PAGE_DEFAULT_NORMAL | SECT_UXN | SECT_RDONLY)
#define PROT_PAGE_DEVICE_nGnRE  	(PROT_PAGE_DEFAULT_DEVICE | SECT_PXN | SECT_UXN)

/*
 * Define the MMU transfer block size:
 * PGD entry size: 512GB -- Translation Level 0
 * PUD entry size: 1GB   -- Translation Level 1
 * PMD entry size: 2MB   -- Translation Level 2
 * PTE entry size: 4KB   -- Translation Level 3
 */
#define PGD_SHIFT	39
#define PGD_SIZE	(_AC(1, UL) << PGD_SHIFT)
#define PGD_MASK	(~(PGD_SIZE-1))
#define PUD_SHIFT	30
#define PUD_SIZE	(_AC(1, UL) << PUD_SHIFT)
#define PUD_MASK	(~(PUD_SIZE-1))
#define PMD_SHIFT	21
#define PMD_SIZE	(_AC(1, UL) << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

void aarch64_setup_memory_mapping(uint8_t *mem, uint64_t mem_size);
void aarch64_mem_size(size_t *mem_size);

#endif /* HVT_CPU_AARCH64_H */
