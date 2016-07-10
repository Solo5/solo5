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
    printf("            |      ___|\n");
    printf("  __|  _ \\  |  _ \\ __ \\\n");
    printf("\\__ \\ (   | | (   |  ) |\n");
    printf("____/\\___/ _|\\___/____/\n");
}


struct solo5_device_t solo5_devices[2] = {
    {
        .poll_event_idx = 0,
        .type = SOLO5_BLK,
        .sync_read = solo5_blk_read_sync,
        .sync_write = solo5_blk_write_sync,
        .async_read = solo5_blk_read_async_submit,
        .async_write = solo5_blk_write_async,
        .async_read_result = solo5_blk_read_async_complete,
        .async_write_result = solo5_blk_write_async_complete,
        .info = NULL
    },
    {
        .poll_event_idx = 1,
        .type = SOLO5_NET,
        .sync_read = solo5_net_read_sync,
        .sync_write = solo5_net_write_sync,
        .info = NULL
    }
};

void kernel_main(struct ukvm_boot_info *bi)
{
    int ret;

    banner();
    printf("mem_size=%lx, kernel_end=%lx\n", bi->mem_size, bi->kernel_end);

    gdt_init();
    interrupts_init();
    interrupts_enable();

    printf("Number of devices: %d\n",
           sizeof(solo5_devices) / sizeof(struct solo5_device_t));

    mem_init(bi->mem_size, bi->kernel_end);

    /* for floating point */
    sse_enable();
    time_init();

    ret = solo5_app_main((char *)bi->cmdline);
    printf("solo5_app_main() returned with %d\n", ret);

    printf("Kernel done.\nGoodbye!\n");
    kernel_hang();
}
