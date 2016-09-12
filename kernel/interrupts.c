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

#include "kernel.h"

#define IDT_NUM_ENTRIES 256
#define IDT_TYPE_INT64 0xe00
#define IDT_TYPE_TRAP64 0xf00
#define IDT_PRESENT 0x8000
#define IDT_HANDLER_SEGMENT (GDT_DESC_OFFSET(GDT_DESC_CODE))
/* which stack in the TSS should be used on interrupt */
#define IDT_IST TSS_IST_INDEX

/* the interrupt map */
#include "interrupts.h"

#define INTR_IS_EXCEPTION(n) ((n) < INTR_IRQ_0)
#define INTR_IS_IRQ(n) (((n) >= INTR_IRQ_0) && ((n) <= INTR_IRQ_15))

struct __attribute__((__packed__)) qw {
    uint32_t lo1;
    uint32_t hi1;
    uint32_t lo2;
    uint32_t hi2;
};

struct __attribute__((__packed__)) idtptr {
    uint16_t limit;
    uint64_t base;
};

/* assembly functions to load IDT  */
extern void idt_load(uint64_t idtptr);

/* The actual memory for the IDT is here */
static struct qw idt[IDT_NUM_ENTRIES] ALIGN_64_BIT;

#define SET_IDT_ENTRY(n, a) do {                                        \
        idt[n].hi2 = 0;                                                 \
        idt[n].lo2 = a >> 32;                                           \
        idt[n].hi1 = (a & 0xffff0000)                                   \
            | IDT_PRESENT | IDT_TYPE_INT64 | IDT_IST;                   \
        idt[n].lo1 = (IDT_HANDLER_SEGMENT << 16) | (a & 0xffff);        \
    } while (0)

#define INTR_STR(x) interrupt##x
#define SET_INTR_IDT_ENTRY(n) do {                                    \
        extern void INTR_STR(n)(void);                                \
        uint64_t intaddr = (uint64_t) &INTR_STR(n);                   \
        SET_IDT_ENTRY(n, intaddr);                                    \
    } while (0)

static void idt_init(void)
{
    /* processor exceptions */
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_DIVIDE_ERROR);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_DEBUG);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_NMI);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_BREAKPOINT);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_OVERFLOW);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_BOUND);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_INVALID_OP);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_DEVICE);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_DBL_FAULT);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_COPROC_SEG);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_INVALID_TSS);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_SEGMENT);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_STACK);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_GPF);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_PF);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_FPU);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_ALIGN);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_MACHINE_CHK);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_SIMD_FP);
    SET_INTR_IDT_ENTRY(INTR_EXCEPTION_VIRT);

    /* remapped PIC irqs */
    SET_INTR_IDT_ENTRY(INTR_IRQ_0);
    SET_INTR_IDT_ENTRY(INTR_IRQ_1);
    SET_INTR_IDT_ENTRY(INTR_IRQ_2);
    SET_INTR_IDT_ENTRY(INTR_IRQ_3);
    SET_INTR_IDT_ENTRY(INTR_IRQ_4);
    SET_INTR_IDT_ENTRY(INTR_IRQ_5);
    SET_INTR_IDT_ENTRY(INTR_IRQ_6);
    SET_INTR_IDT_ENTRY(INTR_IRQ_7);
    SET_INTR_IDT_ENTRY(INTR_IRQ_8);
    SET_INTR_IDT_ENTRY(INTR_IRQ_9);
    SET_INTR_IDT_ENTRY(INTR_IRQ_10);
    SET_INTR_IDT_ENTRY(INTR_IRQ_11);
    SET_INTR_IDT_ENTRY(INTR_IRQ_12);
    SET_INTR_IDT_ENTRY(INTR_IRQ_13);
    SET_INTR_IDT_ENTRY(INTR_IRQ_14);
    SET_INTR_IDT_ENTRY(INTR_IRQ_15);

    /* user defined interrupts */
    SET_INTR_IDT_ENTRY(INTR_USER_1);
    SET_INTR_IDT_ENTRY(INTR_USER_TIMER);
}

/*
 * GDT is defined and loaded by boot.S (virtio) or gdt.c (ukvm).
 */
extern uint64_t cpu_gdt64[];

struct tss {
    uint32_t reserved;
    uint64_t rsp[3];
    uint64_t reserved2;
    uint64_t ist[7];
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t iomap_base;
} __attribute__((packed));

static struct tss cpu_tss;

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

static char cpu_intrstack[4096];
extern void tss_load(uint16_t tss);

static void tss_init(void)
{
    struct tss_desc *td = (void *)&cpu_gdt64[GDT_DESC_TSS_LO];

    cpu_tss.ist[0] = (uint64_t)&cpu_intrstack[sizeof cpu_intrstack];
    td->limit_lo = sizeof(cpu_tss);
    td->base_lo = (uint64_t)&cpu_tss;
    td->type = 0x9;
    td->dpl = 0;
    td->p = 1;
    td->limit_hi = 0;
    td->gran = 0;
    td->base_hi = (uint64_t)&cpu_tss >> 24;
    td->zero = 0;

    tss_load(GDT_DESC_TSS_LO*8);
}

void interrupts_init(void)
{
    struct idtptr idtptr;

    /* initialize IDT "pointer" */
    idtptr.limit = sizeof(idt) - 1;
    idtptr.base = (uint64_t) &idt;

    /* clear structures */
    memset(idt, 0, idtptr.limit);

    idt_init();
    idt_load((uint64_t) &idtptr);
    tss_init();

    low_level_interrupts_init();
}

void interrupt_handler(uint64_t num,
                       uint64_t errorcode,
                       uint64_t rip)
{

    if (INTR_IS_EXCEPTION(num)) {
        printf("exception from rip=0x%lx\n", rip);
        switch (num) {
        case INTR_EXCEPTION_DIVIDE_ERROR:
            PANIC("got divide error (0x%x)\n", num);
            break;
        case INTR_EXCEPTION_DBL_FAULT:
            PANIC("got double fault (0x%x)\n", num);
            break;
        case INTR_EXCEPTION_PF:
            printf("got page fault (0x%x)\n", num);
            PANIC("errorcode was (0x%x)\n", errorcode);
            break;
        case INTR_EXCEPTION_GPF:
            printf("got gpf (0x%x)\n", num);
            PANIC("errorcode was (0x%x)\n", errorcode);
            break;
        default:
            PANIC("got unknown processor exception 0x%x\n", num);
        };


    } else {
        if (INTR_IS_IRQ(num))
            low_level_handle_irq(num - INTR_IRQ_0);
        else
            low_level_handle_intr(num);
    }
}

/* keeps track of how many stacked "interrupts_disable"'s there are */
int spldepth = 1;

void interrupts_disable(void)
{
    __asm__ __volatile__("cli");
    spldepth++;
}

void interrupts_enable(void)
{
    assert(spldepth > 0);

    if (--spldepth == 0)
        __asm__ __volatile__("sti");
}

