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


extern void start_kernel(void);
void sleep_test(void);
void blk_test(void);


static void banner(void) {
    printf("            |      ___|  \n");
    printf("  __|  _ \\  |  _ \\ __ \\  \n");
    printf("\\__ \\ (   | | (   |  ) | \n");
    printf("____/\\___/ _|\\___/____/  \n");
}

/* args are: 
   uint64_t size, uint64_t kernel_end, ...
*/
void kernel_main(uint64_t size, uint64_t kernel_end) {
    banner();
    printf("size=%lx, kernel_end=%lx\n", size, kernel_end);

    gdt_init();
    interrupts_init();
    interrupts_enable();

    mem_init(size, kernel_end);

    /* for floating point */
    sse_enable();
    time_init();

    start_kernel();

    printf("Kernel done. \nGoodbye!\n");
    kernel_hang();
}