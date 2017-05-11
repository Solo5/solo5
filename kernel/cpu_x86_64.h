/*
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a unikernel base layer.
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

#ifndef _BITUL

#ifdef ASM_FILE
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
#define X86_CR0_NE_BIT          5 /* Numeric Exception */
#define X86_CR0_NE              _BITUL(X86_CR0_NE_BIT)
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
#define X86_CR4_VMXE_BIT        13 /* VMX enabled */
#define X86_CR4_VMXE            _BITUL(X86_CR4_VMXE_BIT)

/*
 * Intel CPU features in EFER
 */
#define X86_EFER_LME_BIT        8 /* Long mode enable (R/W) */
#define X86_EFER_LME            _BITUL(X86_EFER_LME_BIT)
#define X86_EFER_LMA_BIT        10 /* Long mode active (R/O) */
#define X86_EFER_LMA            _BITUL(X86_EFER_LMA_BIT)

#define PAGE_SIZE               4096
#define PAGE_SHIFT              12
#define PAGE_MASK               ~(0xfff)

/*
 * GDT layout
 *
 * This should be kept consistent with the layout used by the ukvm target (as
 * defined in ukvm/ukvm_cpu_x86_64.h.
 */
#define GDT_DESC_NULL           0
#define GDT_DESC_CODE           1
#define GDT_DESC_CODE32         2 /* Used by boot.S on virtio targets */
#define GDT_DESC_DATA           3
#define GDT_DESC_TSS_LO         4
#define GDT_DESC_TSS_HI         5
#define GDT_DESC_TSS            GDT_DESC_TSS_LO

#define GDT_DESC_OFFSET(n)      ((n) * 0x8)
#define GDT_NUM_ENTRIES         6

#define GDT_DESC_CODE_VAL       0x00af99000000ffff
#define GDT_DESC_DATA_VAL       0x00cf93000000ffff

#ifndef ASM_FILE
/*
 * The remainder of this file is used only from C.
 */

struct gdtptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

#define IDT_NUM_ENTRIES 48

struct idt_gate_desc {
    uint64_t offset_lo:16;
    uint64_t selector:16;
    uint64_t ist:3;
    uint64_t reserved:5;
    uint64_t type:5;
    uint64_t dpl:2;
    uint64_t p:1;
    uint64_t offset_hi:48;
    uint64_t reserved1:32;
} __attribute__((packed));

struct idtptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct tss {
    uint32_t reserved;
    uint64_t rsp[3];
    uint64_t reserved2;
    uint64_t ist[7];
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t iomap_base;
} __attribute__((packed));

struct tss_desc {
    uint64_t limit_lo:16;
    uint64_t base_lo:24;
    uint64_t type:5;
    uint64_t dpl:2;
    uint64_t p:1;
    uint64_t limit_hi:4;
    uint64_t unused:3;
    uint64_t gran:1;
    uint64_t base_hi:40;
    uint64_t reserved:8;
    uint64_t zero:5;
    uint64_t reserved1:19;
} __attribute__((packed));

struct trap_regs {
    uint64_t cr2;
    uint64_t ec;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

static inline uint64_t cpu_rdtsc(void)
{
    unsigned long l, h;

    __asm__ __volatile__("rdtsc" : "=a"(l), "=d"(h));
    return ((uint64_t)h << 32) | l;
}

static inline uint64_t mul64_32(uint64_t a, uint32_t b)
{
    uint64_t prod;

    __asm__ (
        "mul %%rdx ; "
        "shrd $32, %%rdx, %%rax"
        : "=a" (prod)
        : "0" (a), "d" ((uint64_t)b)
    );

    return prod;
}

/* accessing devices via port space */

static inline void outb(uint16_t port, uint8_t v)
{
    __asm__ __volatile__("outb %0,%1" : : "a" (v), "dN" (port));
}
static inline void outw(uint16_t port, uint16_t v)
{
    __asm__ __volatile__("outw %0,%1" : : "a" (v), "dN" (port));
}
static inline void outl(uint16_t port, uint32_t v)
{
    __asm__ __volatile__("outl %0,%1" : : "a" (v), "dN" (port));
}
static inline uint8_t inb(uint16_t port)
{
    uint8_t v;

    __asm__ __volatile__("inb %1,%0" : "=a" (v) : "dN" (port));
    return v;
}
static inline uint16_t inw(uint16_t port)
{
    uint16_t v;

    __asm__ __volatile__("inw %1,%0" : "=a" (v) : "dN" (port));
    return v;
}
static inline uint32_t inl(uint16_t port)
{
    uint32_t v;

    __asm__ __volatile__("inl %1,%0" : "=a" (v) : "dN" (port));
    return v;
}

static inline uint64_t inq(uint16_t port_lo)
{
    uint16_t port_hi = port_lo + 4;
    uint32_t lo, hi;

    __asm__ __volatile__("inl %1,%0" : "=a" (lo) : "dN" (port_lo));
    __asm__ __volatile__("inl %1,%0" : "=a" (hi) : "dN" (port_hi));

    return ((uint64_t)lo) | ((uint64_t)hi << 32);
}

#endif /* !ASM_FILE */
