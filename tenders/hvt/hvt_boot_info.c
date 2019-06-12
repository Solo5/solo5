/*
 * Copyright (c) 2015-2019 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a sandboxed execution environment.
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

/*
 * hvt_boot_info.c: Guest low memory / struct hvt_boot_info initialisation.
 *
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hvt.h"

static void setup_cmdline(uint8_t *cmdline, int argc, char **argv)
{
    size_t cmdline_free = HVT_CMDLINE_SIZE;

    cmdline[0] = 0;

    for (; *argv; argc--, argv++) {
        size_t alen = snprintf((char *)cmdline, cmdline_free, "%s%s", *argv,
                (argc > 1) ? " " : "");
        if (alen >= cmdline_free) {
            errx(1, "Guest command line too long (max=%d characters)",
                    HVT_CMDLINE_SIZE - 1);
            break;
        }
        cmdline_free -= alen;
        cmdline += alen;
    }
}

void hvt_boot_info_init(struct hvt *hvt, hvt_gpa_t gpa_kend, int cmdline_argc,
        char **cmdline_argv, struct mft *mft, size_t mft_size)
{
    assert(hvt->cpu_boot_info_base);
    assert(hvt->cpu_cycle_freq);
    hvt_gpa_t lowmem_pos = hvt->cpu_boot_info_base;

    /*
     * Start with (struct hvt_boot_info) itself.
     */
    struct hvt_boot_info *bi =
        (struct hvt_boot_info *)(hvt->mem + lowmem_pos);
    lowmem_pos += sizeof (struct hvt_boot_info);
    bi->mem_size = hvt->mem_size;
    bi->kernel_end = gpa_kend;
    bi->cpu_cycle_freq = hvt->cpu_cycle_freq;
    /*
     * Followed by mft_size bytes for manifest.
     *
     * TODO: Sanitize "private but harmless" fields such as hostfd here?
     */
    bi->mft = lowmem_pos;
    memcpy(hvt->mem + lowmem_pos, mft, mft_size);
    lowmem_pos += mft_size;
    /*
     * Followed by HVT_CMDLINE_SIZE bytes for command line.
     */
    bi->cmdline = lowmem_pos;
    setup_cmdline(hvt->mem + lowmem_pos, cmdline_argc, cmdline_argv);
    lowmem_pos += HVT_CMDLINE_SIZE;
}
