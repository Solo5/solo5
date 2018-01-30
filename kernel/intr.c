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

#include "kernel.h"

#define MAX_IRQ_HANDLER_ENTRIES 8

struct irq_handler {
    int (*handler)(void *);
    void *arg;
};

struct irq_handler_list {
    int num_entries;
    struct irq_handler entries[MAX_IRQ_HANDLER_ENTRIES];
};

static struct irq_handler_list irq_handlers[16];

void intr_register_irq(unsigned irq, int (*handler)(void *), void *arg)
{
    assert(irq < 16);
    assert(irq_handlers[irq].num_entries < MAX_IRQ_HANDLER_ENTRIES);

    cpu_intr_disable();

    int idx = irq_handlers[irq].num_entries;
    struct irq_handler *h = &irq_handlers[irq].entries[idx];

    h->handler = handler;
    h->arg = arg;
    irq_handlers[irq].num_entries++;

    cpu_intr_enable();
    platform_intr_clear_irq(irq);
}

void intr_irq_handler(uint64_t irq)
{
    int handled = 0;
    int i;

    for (i = 0; i < irq_handlers[irq].num_entries; i++) {
        struct irq_handler *h = &irq_handlers[irq].entries[i];
        if (h->handler(h->arg) == 1) {
            handled = 1;
            break;
        }
    }

    if (!handled)
        log(ERROR, "Solo5: unhandled irq %d\n", irq);
    else
        /* Only ACK the IRQ if handled; we only need to know about an unhandled
         * IRQ the first time round. */
        platform_intr_ack_irq(irq);
}
