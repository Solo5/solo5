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

char *cmdline_parse(const char *cmdline)
{
    const char opt_quiet[] = "--solo5:quiet";
    const char opt_debug[] = "--solo5:debug";

    const char *p = cmdline;
    bool matched;
    char *after;

    while (*p && isspace(*p))
        p++;

    while (*p) {
        matched = false;
        if (strncmp(p, opt_quiet, (sizeof(opt_quiet) - 1)) == 0) {
            after = (char *) (p + (sizeof(opt_quiet) - 1));
            if (isspace(*after) || *after == '\0') {
                log_set_level(ERROR);
                p += (sizeof(opt_quiet) - 1);
                matched = true;
            }
        }
        else if (strncmp(p, opt_debug, (sizeof(opt_debug) - 1)) == 0) {
            after = (char *) (p + (sizeof(opt_debug) - 1));
            if (isspace(*after) || *after == '\0') {
                log_set_level(DEBUG);
                p += (sizeof(opt_debug) - 1);
                matched = true;
            }
        }
        if (matched) {
            while (*p && isspace(*p))
                p++;
        }
        else {
            break;
        }
    }

    return (char *) p;
}
