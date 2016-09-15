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
#include "queue.h"

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

static struct idt_gate_desc cpu_idt[IDT_NUM_ENTRIES] ALIGN_64_BIT;

static void idt_fillgate(unsigned num, void *fun, unsigned ist)
{
    struct idt_gate_desc *desc = &cpu_idt[num];

    /*
     * All gates are interrupt gates, all handlers run with interrupts off.
     */
    desc->offset_hi = (uint64_t)fun >> 16;
    desc->offset_lo = (uint64_t)fun & 0xffff;
    desc->selector = GDT_DESC_OFFSET(GDT_DESC_CODE);
    desc->ist = ist;
    desc->type = 0b1110;
    desc->dpl = 0;
    desc->p = 1;
}

static void idt_init(void)
{
    /*
     * Load trap vectors. All traps run on IST2 (cpu_trap_stack), except for
     * the exceptions.
     */
#define FILL_TRAP_GATE(num, ist) extern void cpu_trap_##num(void); \
    idt_fillgate(num, cpu_trap_##num, ist)
    FILL_TRAP_GATE(0, 2);
    FILL_TRAP_GATE(1, 2);
    FILL_TRAP_GATE(2, 3); /* #NMI runs on IST3 (cpu_nmi_stack) */
    FILL_TRAP_GATE(3, 2);
    FILL_TRAP_GATE(4, 2);
    FILL_TRAP_GATE(5, 2);
    FILL_TRAP_GATE(6, 2);
    FILL_TRAP_GATE(7, 2);
    FILL_TRAP_GATE(8, 3); /* #DF runs on IST3 (cpu_nmi_stack) */
    FILL_TRAP_GATE(10, 2);
    FILL_TRAP_GATE(11, 2);
    FILL_TRAP_GATE(12, 2);
    FILL_TRAP_GATE(13, 2);
    FILL_TRAP_GATE(14, 2);
    FILL_TRAP_GATE(16, 2);
    FILL_TRAP_GATE(17, 2);
    FILL_TRAP_GATE(18, 2);
    FILL_TRAP_GATE(19, 2);
    FILL_TRAP_GATE(20, 2);

    /*
     * Load irq vectors. All irqs run on IST1 (cpu_intr_stack).
     */
#define FILL_IRQ_GATE(num, ist) extern void cpu_irq_##num(void); \
    idt_fillgate(32 + num, cpu_irq_##num, ist)
    FILL_IRQ_GATE(0, 1);
    FILL_IRQ_GATE(1, 1);
    FILL_IRQ_GATE(2, 1);
    FILL_IRQ_GATE(3, 1);
    FILL_IRQ_GATE(4, 1);
    FILL_IRQ_GATE(5, 1);
    FILL_IRQ_GATE(6, 1);
    FILL_IRQ_GATE(7, 1);
    FILL_IRQ_GATE(8, 1);
    FILL_IRQ_GATE(9, 1);
    FILL_IRQ_GATE(10, 1);
    FILL_IRQ_GATE(11, 1);
    FILL_IRQ_GATE(12, 1);
    FILL_IRQ_GATE(13, 1);
    FILL_IRQ_GATE(14, 1);
    FILL_IRQ_GATE(15, 1);

    struct idtptr idtptr;

    idtptr.limit = sizeof(cpu_idt) - 1;
    idtptr.base = (uint64_t) &cpu_idt;
    cpu_idt_load((uint64_t) &idtptr);
}

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

static struct tss cpu_tss;

static char cpu_intr_stack[4096]; /* IST1 */
static char cpu_trap_stack[4096]; /* IST2 */
static char cpu_nmi_stack[4096];  /* IST3 */

static void tss_init(void)
{
    extern uint64_t cpu_gdt64[];
    struct tss_desc *td = (void *)&cpu_gdt64[GDT_DESC_TSS_LO];

    cpu_tss.ist[0] = (uint64_t)&cpu_intr_stack[sizeof cpu_intr_stack];
    cpu_tss.ist[1] = (uint64_t)&cpu_trap_stack[sizeof cpu_trap_stack];
    cpu_tss.ist[2] = (uint64_t)&cpu_nmi_stack[sizeof cpu_nmi_stack];
    td->limit_lo = sizeof(cpu_tss);
    td->base_lo = (uint64_t)&cpu_tss;
    td->type = 0x9;
    td->dpl = 0;
    td->p = 1;
    td->limit_hi = 0;
    td->gran = 0;
    td->base_hi = (uint64_t)&cpu_tss >> 24;
    td->zero = 0;

    cpu_tss_load(GDT_DESC_TSS_LO*8);
}

void intr_init(void)
{
    tss_init();
    idt_init();
    platform_intr_init();
}

struct trap_regs {
    uint64_t cr2;
    uint64_t ec;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

static char *traps[32] = {
    "#DE", "#DB", "#NMI", "#BP", "#OF", "#BR", "#UD", "#NM", "#DF", "#9", "#TS",
    "#NP", "#SS", "#GP", "#PF", "#15", "#MF", "#AC", "#MC", "#XM", "#VE", "#21",
    "#22", "#23", "#24", "#25", "#26", "#27", "#28", "#29", "#30", "#31"
};

void trap_handler(uint64_t num, struct trap_regs *regs)
{
    printf("trap: type=%s ec=0x%lx rip=0x%lx rsp=0x%lx rflags=0x%lx\n",
        traps[num], regs->ec, regs->rip, regs->rsp, regs->rflags);
    if (num == 14)
        printf("trap: cr2=0x%lx\n", regs->cr2);
    PANIC("Fatal trap");
}

struct irq_handler {
    int (*handler)(void *);
    void *arg;

    SLIST_ENTRY(irq_handler) entries;
};

SLIST_HEAD(irq_handler_head, irq_handler);
static struct irq_handler_head irq_handlers[16];

void intr_register_irq(unsigned irq, int (*handler)(void *), void *arg)
{
    assert (irq < 16);
    struct irq_handler *h = malloc(sizeof (struct irq_handler));
    assert(h != NULL);
    h->handler = handler;
    h->arg = arg;

    intr_disable();
    SLIST_INSERT_HEAD(&irq_handlers[irq], h, entries);
    intr_enable();
    platform_intr_clear_irq(irq);
}

void irq_handler(uint64_t irq)
{
    struct irq_handler *h;
    int handled = 0;

    SLIST_FOREACH(h, &irq_handlers[irq], entries) {
        if (h->handler(h->arg) == 1) {
            handled = 1;
            break;
        }
    }

    if (!handled)
        printf("unhandled irq %d\n", irq);
    else
        /* Only ACK the IRQ if handled; we only need to know about an unhandled
         * IRQ the first time round. */
        platform_intr_ack_irq(irq);
}

/* keeps track of how many stacked "interrupts_disable"'s there are */
int intr_depth = 1;

void intr_disable(void)
{
    __asm__ __volatile__("cli");
    intr_depth++;
}

void intr_enable(void)
{
    assert(intr_depth > 0);

    if (--intr_depth == 0)
        __asm__ __volatile__("sti");
}
