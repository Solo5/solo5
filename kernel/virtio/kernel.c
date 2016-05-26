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

void kernel_main(uint32_t arg)
{
    volatile int gdb = 1;
    serial_init();

    printf("            |      ___|  \n");
    printf("  __|  _ \\  |  _ \\ __ \\  \n");
    printf("\\__ \\ (   | | (   |  ) | \n");
    printf("____/\\___/ _|\\___/____/  \n");

    if (!gdb) printf("looping for gdb\n");
    while ( gdb == 0 ); 

    /* needs to be very early as it clears the bss */
    mem_init((struct multiboot_info *)((uint64_t)arg));

    interrupts_init();
    /* ocaml needs floating point */
    sse_enable();
    time_init();

    pci_enumerate();

    interrupts_enable();

    kernel_postboot();
}
