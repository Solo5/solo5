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
#include "printf.h"

static log_level_t log_level = INFO;

int log(log_level_t level, const char *fmt, ...)
{
    if (log_level < level) {
        return 0;
    }

    va_list args;
    size_t size;
    /*
     * buffer[] might be on an IST stack, so pick an arbitrary-but-not-too-big
     * size (4 display lines).
     */
    char buffer[320];

    va_start(args, fmt);
    size = vsnprintf(buffer, sizeof buffer, fmt, args);
    va_end(args);

    /*
     * If the message doesn't fit into the buffer, at least try and tell the
     * user about it.
     */
    if (size >= sizeof buffer) {
        const char trunc[] = "(truncated)\n";
        platform_puts(buffer, sizeof buffer - 1);
        platform_puts(trunc, sizeof trunc - 1);
        return sizeof buffer - 1;
    } else {
        platform_puts(buffer, size);
        return size;
    }
}

void log_set_level(log_level_t level)
{
    log_level = level;
}
