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

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

int solo5_app_main(const struct solo5_start_info *si)
{
    puts("\n**** Solo5 standalone test_hello ****\n\n");
    puts("Hello, World\nCommand line is: '");

    size_t len = 0;
    const char *p = si->cmdline;

    while (*p++)
        len++;
    solo5_console_write(si->cmdline, len);

    puts("'\n");

    /* "Hello_Solo5" will be passed in via the command line */
    if (strcmp(si->cmdline, "Hello_Solo5") == 0)
        puts("SUCCESS\n");

    return SOLO5_EXIT_SUCCESS;
}
