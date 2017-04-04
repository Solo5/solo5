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

static char *digits = "0123456789abcdef";

#define STRLEN 16
static char strbuf[STRLEN + 1];

void hex_to_str(char *target, uint64_t val) {
    int i;
    for (i = 0; i < STRLEN; i++) {
        int digit = val & 0xf;
        target[STRLEN - i - 1] = digits[digit];
        val = val >> 4;
    }
    target[STRLEN] = '\n';
    
}

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

static inline uint64_t cpu_rdtsc(void)
{
    uint32_t edx_, eax_;

    __asm__("rdtsc" : "=a" (eax_), "=d" (edx_));
    return (uint64_t)eax_ + ((uint64_t)edx_ << 32);
}


#define RDTSC_LOOPS 1000000

int solo5_app_main(char *cmdline __attribute__((unused)))
{
    volatile uint64_t start, end;
    int i;
    
    puts("\n**** Solo5 standalone test_rdtsc ****\n\n");

    start = cpu_rdtsc();
    for (i = 0; i < RDTSC_LOOPS; i++) {
        end = cpu_rdtsc();
    }
    puts("rdtsc diff: ");
    hex_to_str(strbuf, end - start);
    solo5_console_write(strbuf, STRLEN + 1);
    
    return 0;
}
