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

#define COM1 0x3f8   

#define COM1_DATA   (COM1 + 0)
#define COM1_INTR   (COM1 + 1)
#define COM1_CTRL   (COM1 + 3)
#define COM1_STATUS (COM1 + 5)

// only when DLAB is set
#define COM1_DIV_LO (COM1 + 0)
#define COM1_DIV_HI (COM1 + 1)

#define DLAB 0x80
#define PROT 0x03 /* 8N1 (8 bits, no parity, one stop bit) */

static void serial_write(char a) {
    outb(COM1_DATA, a);
}

void serial_putc(char a) {
    if ( a == '\n' )
        serial_write('\r');
    serial_write(a);
}
