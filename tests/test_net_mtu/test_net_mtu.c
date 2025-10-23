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

#include <stdarg.h>
#include <stddef.h>

#include "solo5.h"
#include "../../bindings/lib.c"
#include "../../bindings/printf.c"

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

static void printf(const char *fmt, ...)
    __attribute__ ((format (printf, 1, 2)));

static void printf(const char *fmt, ...)
{
    char buffer[1024];
    va_list args;
    size_t size;

    va_start(args, fmt);
    size = vsnprintf(buffer, sizeof buffer, fmt, args);
    va_end(args);

    if (size >= sizeof buffer) {
        const char trunc[] = "(truncated)\n";
        solo5_console_write(buffer, sizeof buffer - 1);
        solo5_console_write(trunc, sizeof trunc - 1);
    }
    else {
        solo5_console_write(buffer, size);
    }
}

#define HLEN_ETHER  6

static void tohexs(char *dst, uint8_t *src, size_t size)
{
    while (size--) {
        uint8_t n = *src >> 4;
        *dst++ = (n < 10) ? (n + '0') : (n - 10 + 'a');
        n = *src & 0xf;
        *dst++ = (n < 10) ? (n + '0') : (n - 10 + 'a');
        src++;
    }
    *dst = '\0';
}

static bool opt_verbose = false;

static bool print_mtu(void)
{
    solo5_handle_t h;
    struct solo5_net_info info;

    if (solo5_net_acquire("service0", &h, &info) != SOLO5_R_OK) {
        puts("Could not acquire 'service0' network\n");
        return false;
    }

    if (opt_verbose) {
        char macaddr_s[(HLEN_ETHER * 2) + 1];
        tohexs(macaddr_s, info.mac_address, HLEN_ETHER);
        puts(macaddr_s);
        puts("\n");
    }

    printf("MTU is %lu\n", info.mtu);

    return true;
}

int solo5_app_main(const struct solo5_start_info *si)
{
    puts("\n**** Solo5 standalone test_net_mtu ****\n\n");

    if (strlen(si->cmdline) >= 1) {
        switch (si->cmdline[0]) {
        case 'v':
            opt_verbose = true;
            break;
        default:
            puts("Error in command line.\n");
            puts("Usage: test_net_mtu [ verbose ]\n");
            return SOLO5_EXIT_FAILURE;
        }
    }

    if (print_mtu()) {
        puts("SUCCESS\n");
        return SOLO5_EXIT_SUCCESS;
    }
    else {
        puts("FAILURE\n");
        return SOLO5_EXIT_FAILURE;
    }
}
