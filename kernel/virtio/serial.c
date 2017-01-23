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

#define COM1 0x3f8

#define COM1_DATA   (COM1 + 0)
#define COM1_INTR   (COM1 + 1)
#define COM1_CTRL   (COM1 + 3)
#define COM1_STATUS (COM1 + 5)

/* only when DLAB is set */
#define COM1_DIV_LO (COM1 + 0)
#define COM1_DIV_HI (COM1 + 1)

#define DLAB 0x80
#define PROT 0x03 /* 8N1 (8 bits, no parity, one stop bit) */

void serial_init(void)
{
	outb(COM1_INTR, 0x00);      /* Disable all interrupts */
	outb(COM1_CTRL, DLAB);      /* Enable DLAB (set baud rate divisor) */
	outb(COM1_DIV_LO, 0x01);    /* Set divisor to 1 (lo byte) 115200 baud */
	outb(COM1_DIV_HI, 0x00);    /*                  (hi byte) */
	outb(COM1_CTRL, PROT);      /* Set 8N1, clear DLAB */
}


static int serial_tx_empty(void)
{
	return inb(COM1_STATUS) & 0x20;
}

static void serial_write(char a)
{
    while (!serial_tx_empty())
        ;

    outb(COM1_DATA, a);
}

void serial_putc(char a)
{
    if (a == '\n')
        serial_write('\r');
    serial_write(a);
}
