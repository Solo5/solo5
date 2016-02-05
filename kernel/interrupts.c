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
/* Interrupt handling on the x86_64 involves setting up the IDT
 * (interrupt descriptor table) to point to handlers that are called
 * when the processor generates an interrupt.
 *
 * The set up of the IDT is intimately related to the the GDT (global
 * descriptor table) and TSS (task segment selector), which have been
 * set up for us by the loader.
 *
 * For 64-bit execution, we want to have a NULL SS (segments aren't
 * used anyway and seem to confuse the CPU on returning from
 * interrupts).  To do this we change the SS value on the stack before
 * returning from the first interrupt at setup time.  It's a bit of a
 * nasty hack, called the "SS hack" below.
 */

#define IDT_NUM_ENTRIES 256
#define IDT_TYPE_INT64 0xe00   
#define IDT_TYPE_TRAP64 0xf00
#define IDT_PRESENT 0x8000
#define IDT_HANDLER_SEGMENT (GDT_DESC_OFFSET(GDT_DESC_CODE))
/* which stack in the TSS should be used on interrupt */
#define IDT_IST TSS_IST_INDEX

/* the interrupt map */
#include "interrupts.h"

#define INTR_IRQ_MASTER INTR_IRQ_0
#define INTR_IRQ_SLAVE  INTR_IRQ_8

#define INTR_IS_EXCEPTION(n) ((n) < INTR_IRQ_0)
#define INTR_IS_IRQ(n) (((n) >= INTR_IRQ_0) && ((n) <= INTR_IRQ_15))

void irq_eoi(unsigned char irq);

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

/* executed when PIT fires (i.e. when IRQ0 is fired) */
extern void increment_time_count(void);

/* The actual memory for the IDT is here */
struct qw idt[IDT_NUM_ENTRIES] ALIGN_64_BIT;

#define SET_IDT_ENTRY(n, a) do {                                        \
        idt[n].hi2 = 0;                                                 \
        idt[n].lo2 = a >> 32;                                           \
        idt[n].hi1 = (a & 0xffff0000)                                   \
            | IDT_PRESENT | IDT_TYPE_INT64 | IDT_IST;                   \
        idt[n].lo1 = (IDT_HANDLER_SEGMENT << 16) | (a & 0xffff);        \
    } while(0)

#define INTR_STR(x) interrupt##x
#define SET_INTR_IDT_ENTRY(n) do {                                    \
        extern void INTR_STR(n)(void);                                \
        uint64_t intaddr = (uint64_t) &INTR_STR(n);                   \
        SET_IDT_ENTRY(n, intaddr);                                    \
    } while(0)

/* The SS_HACK entry is for the ugly hack to get the SS to have a null
   descriptor. */
#define SET_SS_HACK_IDT_ENTRY(n) do {                                       \
        extern void ss_interrupt(void);                                \
        uint64_t intaddr = (uint64_t) &ss_interrupt;                   \
        SET_IDT_ENTRY(n, intaddr);                                      \
    } while(0);


static void idt_init(void) {
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

    /* SS hack */
    SET_SS_HACK_IDT_ENTRY(INTR_SS_HACK);

    /* user defined interrupts */
    SET_INTR_IDT_ENTRY(INTR_USER_1);
	SET_INTR_IDT_ENTRY(INTR_TIMER);
}

void interrupts_init(void) {
    struct idtptr idtptr;

    /* initialize IDT "pointer" */
    idtptr.limit = sizeof(idt) - 1;
    idtptr.base = (uint64_t) &idt;

    /* clear structures */
    memset(idt, 0, idtptr.limit);

    idt_init();
    idt_load((uint64_t) &idtptr);
}

void interrupt_handler(uint64_t num,
                       uint64_t errorcode,
                       uint64_t rip) {

    if ( INTR_IS_EXCEPTION(num) ) {
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


    } else if ( INTR_IS_IRQ(num) ) {
        int irq = num - INTR_IRQ_0;
        
        switch (irq) {
            case 0xB:
                handle_virtio_interrupt();
                break;
            case 0:
                increment_time_count();
                break;
            default:
                printf("got irq %d at 0x%lx\n", irq, 
                        time_monotonic_ms() );

        }

        irq_eoi(irq);
    } else {

        switch (num) {
		case INTR_TIMER:
			//increment_ms_count();
			printf("T(0x%x)\n", num);
			break;
        case INTR_USER_1:
            printf("got user interrupt (0x%x)\n", num);
            break;
        default:
            PANIC("got unknown processor exception 0x%x\n", num);
        };
    }

}

#define PIC1 0x20   /* IO base address for master PIC */
#define PIC2 0xA0   /* IO base address for slave PIC */
#define PIC1_COMMAND PIC1
#define PIC1_DATA (PIC1 + 1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA (PIC2 + 1)

#define IRQ_ON_MASTER(n) ((n) < 8)
#define IRQ_PORT(n)      (IRQ_ON_MASTER(n) ? PIC1_DATA : PIC2_DATA)
#define IRQ_OFFSET(n)    (IRQ_ON_MASTER(n) ? (n) : ((n) - 8))


#define PIC_EOI 0x20/* End-of-interrupt command code */
 
void irq_eoi(unsigned char irq)
{
    if(!IRQ_ON_MASTER(irq))
        outb(PIC2_COMMAND, PIC_EOI);
 
    outb(PIC1_COMMAND, PIC_EOI);
}

void irq_mask(uint8_t irq) {
    uint16_t port;

    port = IRQ_PORT(irq);
    outb(port, inb(port) | (1 << IRQ_OFFSET(irq)));
}

void irq_clear(uint8_t irq) {
    uint16_t port;

    port = IRQ_PORT(irq);
    outb(port, inb(port) & ~(1 << IRQ_OFFSET(irq)));
}



#if 0
static void disable_pic(void) {
    outb(PIC1_DATA, 0xff);
    outb(PIC2_DATA, 0xff);
    printf("PIC disabled\n");
}
#endif

/* reinitialize the PIC controllers, giving them specified vector offsets
   rather than 8h and 70h, as configured by default */
 
#define ICW1_ICW4 0x01/* ICW4 (not) needed */
#define ICW1_SINGLE 0x02/* Single (cascade) mode */
#define ICW1_INTERVAL 40x04/* Call address interval 4 (8) */
#define ICW1_LEVEL 0x08/* Level triggered (edge) mode */
#define ICW1_INIT 0x10/* Initialization - required! */
 
#define ICW4_8086 0x01/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO 0x02/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE 0x08/* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C/* Buffered mode/master */
#define ICW4_SFN M0x10/* Special fully nested (not) */
 
/*
arguments:
offset1 - vector offset for master PIC
vectors on the master become offset1..offset1+7
offset2 - same for slave PIC: offset2..offset2+7
*/
#define io_wait() do { } while(0)

#if 0
static void PIC_remap(int offset1, int offset2)
{
    unsigned char a1, a2;
 
    a1 = inb(PIC1_DATA);                        // save masks
    a2 = inb(PIC2_DATA);
 
    outb(PIC1_COMMAND, ICW1_INIT+ICW1_ICW4);  // starts the initialization sequence (in cascade mode)
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT+ICW1_ICW4);
    io_wait();
    outb(PIC1_DATA, offset1);                 // ICW2: Master PIC vector offset
    io_wait();
    outb(PIC2_DATA, offset2);                 // ICW2: Slave PIC vector offset
    io_wait();
    outb(PIC1_DATA, 4);                       // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
    io_wait();
    outb(PIC2_DATA, 2);                       // ICW3: tell Slave PIC its cascade identity (0000 0010)
    io_wait();
 
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();
 
    outb(PIC1_DATA, a1);   // restore saved masks.
    outb(PIC2_DATA, a2);
}

#endif
