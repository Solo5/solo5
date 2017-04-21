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

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define PAGE_MASK ~(0xfff)

#define GDT_NUM_ENTRIES 6
#define GDT_DESC_NULL 0
#define GDT_DESC_CODE 1
/* 2 == unused / 32-bit bootstrap */
#define GDT_DESC_DATA 3
#define GDT_DESC_TSS_LO  4
#define GDT_DESC_TSS_HI  5
#define GDT_DESC_TSS  GDT_DESC_TSS_LO
#define GDT_DESC_OFFSET(n) ((n) * 0x8)

/* granularity (23), long mode (21), present (15),
 * always 1 (12, 11), readable (9), limit (16-19)
 */
#define GDT_DESC_CODE_VAL (0x00af9a000000ffff)
/* granularity (23), big data seg (22), present (15),
 * type data rw (9), limit (16-19)
 */
#define GDT_DESC_DATA_VAL (0x00cf92000000ffff)

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
