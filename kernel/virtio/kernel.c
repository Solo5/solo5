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

extern void bounce_stack(uint64_t stack_start, void (*tramp)(void));
static void kernel_main2(void) __attribute__((noreturn));

static char cmdline[8192];

void kernel_main(uint32_t arg)
{
    volatile int gdb = 1;

    serial_init();

    printf("            |      ___|\n");
    printf("  __|  _ \\  |  _ \\ __ \\\n");
    printf("\\__ \\ (   | | (   |  ) |\n");
    printf("____/\\___/ _|\\___/____/\n");

    if (!gdb)
        printf("looping for gdb\n");
    while (gdb == 0)
        ;

    /*
     * The multiboot structures may be anywhere in memory, so take a copy of
     * the command line before we initialise memory allocation.
     */
    struct multiboot_info *mi = (struct multiboot_info *)(uint64_t)arg;

    if (mi->flags & MULTIBOOT_INFO_CMDLINE) {
        char *mi_cmdline = (char *)(uint64_t)mi->cmdline;
        size_t cmdline_len = strlen(mi_cmdline);

        /*
         * Skip the first token in the cmdline as it is an opaque "name" for
         * the kernel coming from the bootloader.
         */
        for (; *mi_cmdline; mi_cmdline++, cmdline_len--) {
            if (*mi_cmdline == ' ') {
                mi_cmdline++;
                cmdline_len--;
                break;
            }
        }

        if (cmdline_len >= sizeof(cmdline)) {
            cmdline_len = sizeof(cmdline) - 1;
            printf("kernel_main: warning: command line too long, truncated\n");
        }
        memcpy(cmdline, mi_cmdline, cmdline_len);
    } else {
        cmdline[0] = 0;
    }

    /*
     * Initialise memory map, then immediately switch stack to top of RAM.
     * Indirectly calls kernel_main2().
     */
    mem_init(mi);
    bounce_stack(mem_max_addr(), kernel_main2);
}

static void kernel_main2(void)
{
    int ret;

    intr_init();
    /* ocaml needs floating point */
    cpu_sse_enable();
    time_init();

    pci_enumerate();

    intr_enable();

    ret = solo5_app_main(cmdline);
    printf("solo5_app_main() returned with %d\n", ret);

    platform_exit();
}
