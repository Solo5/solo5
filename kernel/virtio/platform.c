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

static char cmdline[8192];

#define PLATFORM_MEM_START 0x100000
#define PLATFORM_MAX_MEM_SIZE 0x40000000

static uint64_t mem_size;

void platform_init(void *arg)
{
    /*
     * The multiboot structures may be anywhere in memory, so take a copy of
     * the command line before we initialise memory allocation.
     */
    struct multiboot_info *mi = (struct multiboot_info *)arg;

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
            log(WARN, "Solo5: warning: command line too long, truncated\n");
        }
        memcpy(cmdline, mi_cmdline, cmdline_len);
    } else {
        cmdline[0] = 0;
    }

    /*
     * Look for the first chunk of memory at PLATFORM_MEM_START.
     */
    multiboot_memory_map_t *m;
    uint32_t offset;

    for (offset = 0; offset < mi->mmap_length;
            offset += m->size + sizeof(m->size)) {
        m = (void *)(uintptr_t)(mi->mmap_addr + offset);
        if (m->addr == PLATFORM_MEM_START &&
                m->type == MULTIBOOT_MEMORY_AVAILABLE) {
            break;
        }
    }
    assert(offset < mi->mmap_length);

    /*
     * Cap our memory size to PLATFORM_MAX_MEM_SIZE which boot.S defines page
     * tables for.
     */
    mem_size = m->addr + m->len;
    if (mem_size > PLATFORM_MAX_MEM_SIZE)
        mem_size = PLATFORM_MAX_MEM_SIZE;

    platform_intr_init();
}

const char *platform_cmdline(void)
{
    return cmdline;
}

uint64_t platform_mem_size(void)
{
    return mem_size;
}

void platform_exit(int status __attribute__((unused)))
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
    platform_puts("Solo5: Halted\n", 14);
    cpu_halt();
}

int platform_puts(const char *buf, int n)
{
    int i;

    for (i = 0; i < n; i++)
        serial_putc(buf[i]);

    return n;
}

void solo5_console_write(const char *buf, size_t size)
{
    (void)platform_puts(buf, size);
}
