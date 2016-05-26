/* Copyright (c) 2016, IBM 
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
#include "../interrupts.h"

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

void low_level_interrupts_init(void) {
}

void low_level_handle_irq(int irq) {
    switch (irq) {
    case 0:
        //increment_time_count();
        break;
    default:
        printf("got irq %d at 0x%lx\n", irq, 
               solo5_clock_monotonic() );
        
    }

    irq_eoi(irq);
}

void low_level_handle_intr(int num) {
    switch (num) {
    case INTR_USER_TIMER:
        printf("I");
        break;
    case INTR_USER_1:
        printf("got user interrupt (0x%x)\n", num);
        break;
    default:
        PANIC("got unknown processor exception 0x%x\n", num);
    };
}
