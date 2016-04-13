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

static void banner(void)
{
    printf("            |      ___|  \n");
    printf("  __|  _ \\  |  _ \\ __ \\  \n");
    printf("\\__ \\ (   | | (   |  ) | \n");
    printf("____/\\___/ _|\\___/____/  \n");
}

extern void start_kernel(void);

void sleep_test(void);
void blk_test(void);

void kernel_main(uint64_t size, uint64_t kernel_end)
{
	banner();

	gdt_init();
    interrupts_init();
    interrupts_enable();

    mem_init(size, kernel_end);

    /* ocaml needs floating point */
    sse_enable();

	time_init();

	//void ping_forever(void);
	//ping_forever();

    start_kernel();

    //ping_serve();
	//for(;;);


#if 0
    blk_test();
    sleep_test();

    for(;;) {
        ping_serve();  /* does things if network packet comes in */

        /* need atomic condition check to do the wait */
        //kernel_wait();
    }
#endif
    
    printf("Kernel done. \nGoodbye!\n");
    kernel_hang();
}
