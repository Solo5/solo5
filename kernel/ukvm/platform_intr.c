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

/*
 * Interrupts are not used on the ukvm platform, hence these are stubs.
 */

void low_level_interrupts_init(void)
{
}

void intr_ack_irq(unsigned irq __attribute__((unused)))
{
}

void intr_mask_irq(unsigned irq __attribute__((unused)))
{
}

void intr_clear_irq(unsigned irq __attribute__((unused)))
{
}
