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
 * hvt_cpu_aarch64.h: CPU constants and initialisation data common to aarch64
 * backend implementations.
 */

#ifndef HVT_CPU_AARCH64_H
#define HVT_CPU_AARCH64_H

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
 * 0x012000    PMD
 * 0x011000    PUD
 * 0x010000    PGD, memory start for page table
 *   ...       command line arguments
 * 0x002000    hvt_boot_info
 * 0x001000    non-cacheable page
 * 0x000000    unused ram
 */
#define AARCH64_MMIO_BASE       _AC(0x100000000, UL)
#define AARCH64_MMIO_SZ         _AC(0x40000000, UL)
#define AARCH64_GUEST_MIN_BASE  _AC(0x100000, UL)
#define AARCH64_PAGE_TABLE      _AC(0x10000, UL)
#define AARCH64_CMDLINE_BASE    _AC(0xC000, UL)
#define AARCH64_CMDLINE_SZ      (AARCH64_PAGE_TABLE - AARCH64_CMDLINE_BASE)
#define AARCH64_BOOT_INFO       _AC(0x1000, UL)
#define AARCH64_BOOT_INFO_SZ    (AARCH64_CMDLINE_BASE - AARCH64_BOOT_INFO)

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

#define PROT_SECT_DEFAULT       (PGT_DESC_TYPE_SECT | SECT_AF | SECT_S)
#define PROT_SECT_NORMAL        (PROT_SECT_DEFAULT | SECT_PXN | SECT_UXN | ATTRINDX(MT_NORMAL))
#define PROT_SECT_NORMAL_EXEC   (PROT_SECT_DEFAULT | SECT_UXN | ATTRINDX(MT_NORMAL))
#define PROT_SECT_DEVICE_nGnRE  (PROT_SECT_DEFAULT | SECT_PXN | SECT_UXN | ATTRINDX(MT_DEVICE_nGnRE))
#define PROT_PAGE_NORMAL_EXEC   (PGT_DESC_TYPE_PAGE | SECT_AF | SECT_S | SECT_UXN | ATTRINDX(MT_NORMAL))
#define PROT_PAGE_DEVICE_nGnRE  (PGT_DESC_TYPE_PAGE | SECT_AF | SECT_S | SECT_PXN | SECT_UXN | ATTRINDX(MT_DEVICE_nGnRE))

/*
 * Define the MMU transfer block size:
 * PGD entry size: 512GB -- Translation Level 0
 * PUD entry size: 1GB   -- Translation Level 1
 * PMD entry size: 2MB   -- Translation Level 2
 * PTE entry size: 64KB  -- Translation Level 3
 */
#define PGD_SIZE    (_AC(1, UL) << 39)
#define PGD_MASK	(~(PGD_SIZE-1))
#define PUD_SIZE    (_AC(1, UL) << 30)
#define PUD_MASK	(~(PUD_SIZE-1))
#define PMD_SIZE    (_AC(1, UL) << 21)
#define PMD_MASK	(~(PMD_SIZE-1))

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

struct pmd {
    uint64_t entry[1];
};

struct pud {
    uint64_t entry[1];
};

struct pgd {
    uint64_t entry[1];
};

void aarch64_setup_memory_mapping(uint8_t *va_addr, uint64_t mem_size, uint64_t space_size);
void aarch64_mem_size(size_t *mem_size);

#endif /* HVT_CPU_AARCH64_H */
