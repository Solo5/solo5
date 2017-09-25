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

#include "solo5.h"
#include "../../kernel/lib.c"

#if defined(__x86_64__)
#include "../test_exec/test_globals_ukvm_x86_64.h"
#endif

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

int solo5_app_main(char *cmdline __attribute__((unused)))
{
    uint64_t bad_ptr;

    puts("\n**** Solo5 standalone test_exec_failure ***\n\n");

#if defined(__x86_64__)
    /* Let's make hdr.e_phoff (program headers table) an invalid offset (even
     * after an overflow) */
    bad_ptr = 0xffffffffffffffff;
    memcpy(&test_globals_ukvm[32], &bad_ptr, sizeof(uint64_t));

    /* This should fail on the ukvm side */
    solo5_exec(test_globals_ukvm, sizeof(test_globals_ukvm));
#else
    /* Let's fake an abort (so this succeeds) on aarch64 for now. */
    puts("ABORT");
#endif

    puts("Not reached\n");

    return 0;
}
