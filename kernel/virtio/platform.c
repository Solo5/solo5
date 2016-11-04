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

void platform_exit(void)
{
    /*
     * Poke the QEMU "isa-debug-exit" device to "shutdown". Should be harmless
     * if it is not present. This is used to enable automated tests on virtio.
     * Note that the actual QEMU exit() status will be 83 ('S', 41 << 1 | 1).
     */
    outw(0x501, 41);

    /*
     * If we got here, there is no way to initiate "shutdown" on virtio without
     * ACPI, so just halt.
     */
    platform_puts("Solo5: Halted\n", 15);
    cpu_halt();
}

int platform_puts(const char *buf, int n)
{
    int i;

    for (i = 0; i < n; i++)
        serial_putc(buf[i]);

    return n;
}

int solo5_console_write(const char *, size_t)
    __attribute__ ((alias("platform_puts")));
