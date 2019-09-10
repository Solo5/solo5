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

#include "solo5.h"
#include "../../bindings/lib.c"

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

int solo5_app_main(const struct solo5_start_info *si __attribute__((unused)))
{
    puts("\n**** Solo5 standalone test_mft_maxdevices ****\n\n");

    solo5_handle_t storage0, storage62;
    struct solo5_block_info bi; 

    if (solo5_block_acquire("storage0", &storage0, &bi) != SOLO5_R_OK) {
        puts("FAILURE\n");
        return SOLO5_EXIT_FAILURE;
    }

    /*
     * We could test storage1 ... storage61 here, but the point of this test is
     * mainly to exercise the manifest generation and validation code, so we
     * won't bother.
     */

    if (solo5_block_acquire("storage62", &storage62, &bi) != SOLO5_R_OK) {
        puts("FAILURE\n");
        return SOLO5_EXIT_FAILURE;
    }

    puts("SUCCESS\n");
    return SOLO5_EXIT_SUCCESS;
}
