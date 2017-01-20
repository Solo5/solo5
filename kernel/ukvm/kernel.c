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

void _start(struct ukvm_boot_info *bi)
{
    int ret;

    printf("            |      ___|\n");
    printf("  __|  _ \\  |  _ \\ __ \\\n");
    printf("\\__ \\ (   | | (   |  ) |\n");
    printf("____/\\___/ _|\\___/____/\n");

    /* It appears that on macosx Hypervisor.framework, the mxcsr is
     * not started with its default value of 0x1f80.  This results in
     * floating point related exceptions (e.g., denormal numbers)
     * being raised to the guest which would normally be masked and
     * dealt with by hardware (e.g., by rounding).  It is unclear
     * whether this is a problem with Hypervisor.framework or the way
     * that `uhvf` sets up the VMX context.
     *
     * A workaround is for the guest to explicitly set the MXCSR to
     * the default value (0x1f80).
     */
    unsigned default_mxcsr = 0x1f80;
    __asm__ __volatile__("ldmxcsr %0\n" : : "m"(default_mxcsr));

    //gdt_init();
    mem_init(bi->mem_size, bi->kernel_end);
    //intr_init();

    /* for floating point */
    //cpu_sse_enable();
    time_init();

    intr_enable();

    ret = solo5_app_main((char *)bi->cmdline);
    printf("Solo5: solo5_app_main() returned with %d\n", ret);

    platform_exit();
}
