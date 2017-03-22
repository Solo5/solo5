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

#ifndef __UKVM_CPU_X86_64_H__
#define __UKVM_CPU_X86_64_H__

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
 * Basic CPU control in CR0
 */
#define X86_CR0_PE_BIT          0 /* Protection Enable */
#define X86_CR0_PE              _BITUL(X86_CR0_PE_BIT)
#define X86_CR0_MP_BIT          1 /* Monitor Coprocessor */
#define X86_CR0_MP              _BITUL(X86_CR0_MP_BIT)
#define X86_CR0_EM_BIT          2 /* Emulation */
#define X86_CR0_EM              _BITUL(X86_CR0_EM_BIT)
#define X86_CR0_PG_BIT          31 /* Paging */
#define X86_CR0_PG              _BITUL(X86_CR0_PG_BIT)

/*
 * Intel CPU features in CR4
 */
#define X86_CR4_PAE_BIT         5 /* enable physical address extensions */
#define X86_CR4_PAE             _BITUL(X86_CR4_PAE_BIT)
#define X86_CR4_OSFXSR_BIT      9 /* OS support for FXSAVE/FXRSTOR */
#define X86_CR4_OSFXSR          _BITUL(X86_CR4_OSFXSR_BIT)
#define X86_CR4_OSXMMEXCPT_BIT  10 /* OS support for FP exceptions */
#define X86_CR4_OSXMMEXCPT      _BITUL(X86_CR4_OSXMMEXCPT_BIT)

/* 
 * Intel CPU features in EFER
 */
#define X86_EFER_LME_BIT        8 /* Long mode enable (R/W) */
#define X86_EFER_LME            _BITUL(X86_EFER_LME_BIT)
#define X86_EFER_LMA_BIT        10 /* Long mode active (R/O) */
#define X86_EFER_LMA            _BITUL(X86_EFER_LMA_BIT)

/*
 * Intel long mode page directory/table entries
 */
#define X86_PDPT_P_BIT          0 /* Present */
#define X86_PDPT_P              _BITUL(X86_PDPT_P_BIT)
#define X86_PDPT_RW_BIT         1 /* Writable */
#define X86_PDPT_RW             _BITUL(X86_PDPT_RW_BIT)
#define X86_PDPT_PS_BIT         7 /* Page size */
#define X86_PDPT_PS             _BITUL(X86_PDPT_PS_BIT)

/*
 * Abstract x86 segment descriptor.
 */
struct x86_seg {
    uint64_t base;
    uint32_t limit;
    uint8_t type;
    uint8_t p, dpl, db, s, l, g, avl;
};

static const struct x86_seg ukvm_x86_seg_code = {
    .base = 0,
    .limit = 0xfffff,
    .type = 8, .p = 1, .dpl = 0, .db = 0, .s = 1, .l = 1, .g = 1
};

static const struct x86_seg ukvm_x86_seg_data = {
    .base = 0,
    .limit = 0xfffff,
    .type = 2, .p = 1, .dpl = 0, .db = 1, .s = 1, .l = 0, .g = 1
};
 
/*
 * x86 segment descriptor as seen by the CPU in the GDT.
 */
struct x86_gdt_desc {
    uint64_t limit_lo:16;
    uint64_t base_lo:24;
    uint64_t type:4;
    uint64_t s:1;
    uint64_t dpl:2;
    uint64_t p:1;
    uint64_t limit_hi:4;
    uint64_t avl:1;
    uint64_t l:1;
    uint64_t db:1;
    uint64_t g:1;
    uint64_t base_hi:8;
} __attribute__((packed));

enum x86_gdt_selector {
    X86_GDT_NULL,
    X86_GDT_CODE,
    X86_GDT_DATA,
    X86_GDT_TSS_LO,
    X86_GDT_TSS_HI,
    X86_GDT_MAX
};

#define X86_GDTR_LIMIT ((sizeof (struct x86_gdt_desc) * X86_GDT_MAX) - 1)

/*
 * Monitor memory map for x86 CPUs:
 *
 * (unused) X86_ZEROPAGE_BASE   0x0
 * (unused) X86_ZEROPAGE_SIZE   0x1000
 */
#define X86_GDT_BASE            0x1000
#define X86_GDT_SIZE            0x1000
#define X86_PML4_BASE           0x2000
#define X86_PML4_SIZE           0x1000
#define X86_PDPTE_BASE          0x3000
#define X86_PDPTE_SIZE          0x1000
#define X86_PDE_BASE            0x4000
#define X86_PDE_SIZE            0x1000
#define X86_BOOT_INFO_BASE      0x5000
#define X86_BOOT_INFO_SIZE      0x1000
#define X86_CMDLINE_BASE        0x6000
#define X86_CMDLINE_SIZE        0x2000
#define X86_GUEST_MIN_BASE      0x8000

#define X86_GUEST_PAGE_SIZE     0x200000

#define X86_INIT_EFER_SET       X86_EFER_LME
#define X86_INIT_CR0_SET        (X86_CR0_PE | X86_CR0_PG)
#define X86_INIT_CR0_CLEAR      X86_CR0_EM
#define X86_INIT_CR3            X86_PML4_BASE
#define X86_INIT_CR4_SET        (X86_CR4_PAE | X86_CR4_OSFXSR | \
                                X86_CR4_OSXMMEXCPT)
#define X86_INIT_RFLAGS         0x2    /* rflags bit 1 is reserved, must be 1 */

void ukvm_x86_setup_pagetables(uint8_t *mem, size_t mem_size);
void ukvm_x86_setup_gdt(uint8_t *mem);

#endif
