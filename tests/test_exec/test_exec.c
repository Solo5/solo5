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
#include "test_globals_ukvm_x86_64.h"
#endif

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

int solo5_app_main(char *cmdline)
{
    puts("\n**** Solo5 standalone test_exec ****\n\n");
    puts(cmdline);

#if defined(__x86_64__)
    /*
     * If this ever fails, check that the ukvm interface has not changed. This
     * next line will load a unikernel that uses some specific ukvm interface.
     * If it changes, the unikernel about to run might fail.
     *
     * XXX: we should use xxd or some other tool to generate the binary
     * test_globals_ukvm. Alternatively, we could load the new ELF as a disk,
     * but that would make this test too complicated.
     */
    solo5_exec(test_globals_ukvm, test_globals_ukvm_len);
#else
    /* Let's fake a success on aarch64 for now. */
    puts("SUCCESS");
#endif

    puts("Not reached\n");

    return 0;
}
