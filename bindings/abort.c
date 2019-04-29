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

#include "bindings.h"

/*
 * These functions deliberately do not call printf() or malloc() in order to
 * abort as quickly as possible without triggering further errors.
 */

static void puts(const char *s)
{
    (void)platform_puts(s, strlen(s));
}

void _assert_fail(const char *file, const char *line, const char *e)
{
    puts("Solo5: ABORT: ");
    puts(file);
    puts(":");
    puts(line);
    puts(": Assertion `");
    puts(e);
    puts("' failed\n");
    platform_exit(SOLO5_EXIT_ABORT, NULL);
}

void _abort(const char *file, const char *line, const char *s, void *regs_hint)
{
    puts("Solo5: ABORT: ");
    puts(file);
    puts(":");
    puts(line);
    puts(": ");
    puts(s);
    puts("\n");
    platform_exit(SOLO5_EXIT_ABORT, regs_hint);
}
