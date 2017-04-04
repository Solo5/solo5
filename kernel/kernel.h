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

#ifndef __KERNEL_H__
#define __KERNEL_H__

#include "solo5.h"
#include "cpu.h"

/* This is the main header file for everything in the kernel */


/* common headers from the x86_64-elf gcc cross compiler */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

/* memory defines */
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define PAGE_MASK ~(0xfff)

/* convenient macro stringification */
#define STR_EXPAND(y) #y
#define STR(x) STR_EXPAND(x)

/* abort.c */
void _assert_fail(const char *, const char *, const char *)
    __attribute__((noreturn));
void _abort(const char *, const char *, const char *)
    __attribute__((noreturn));

#define PANIC(s)                            \
    do {                                    \
        _abort(__FILE__, STR(__LINE__), s); \
    } while (0)
#define assert(e)                                      \
    do {                                               \
        if (!(e))                                      \
            _assert_fail(__FILE__, STR(__LINE__), #e); \
    } while (0)

/* malloc.c: memory allocation */
void *malloc(size_t bytes);
void  free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void *memalign(size_t alignment, size_t bytes);

/* time.c: clocksource */
void time_init(void);

/* ee_printf.c: a third-party printf slightly modified and with
 *              snprintf added
 */
int printf(const char *fmt, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

/* lib.c: expect this to grow... */
void *memset(void *dest, int c, size_t n);
void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *vl, const void *vr, size_t n);
int strcmp(const char *l, const char *r);
char *strcpy(char *restrict dest, const char *restrict src);
size_t strlen(const char *s);

/* platform.c: specifics for ukvm or virito platform */
void platform_exit(void) __attribute__((noreturn));

/* pvclock.c: KVM paravirtualized clock */
int pvclock_init(void);
uint64_t pvclock_monotonic(void);
uint64_t pvclock_epochoffset(void);

#define NSEC_PER_SEC	1000000000ULL

#endif
