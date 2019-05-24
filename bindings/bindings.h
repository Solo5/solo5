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

/*
 * bindings.h: Solo5 bindings, primary header file and internal interfaces.
 *
 * This header file is included by all bindings, and apart from the common
 * internal interfaces includes and defines all necessary C99 runtime
 * interfaces used by bindings.
*/

#ifndef __BINDINGS_H__
#define __BINDINGS_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include "solo5.h"

#ifdef __x86_64__
#include "cpu_x86_64.h"
#elif defined(__aarch64__)
#include "cpu_aarch64.h"
#elif defined(__powerpc64__)
#include "cpu_ppc64.h"
#else
#error Unsupported architecture
#endif

/* alignment macros */
#define ALIGN_4K __attribute__((aligned(0x1000)))
#define ALIGN_64_BIT __attribute__((aligned(0x8)))

/* convenient macro stringification */
#define STR_EXPAND(y) #y
#define STR(x) STR_EXPAND(x)

#if defined(__OpenBSD__)
#define SSP_GUARD __guard_local
#define SSP_FAIL __stack_smash_handler
#else
#define SSP_GUARD __stack_chk_guard
#define SSP_FAIL __stack_chk_fail
#endif

/* abort.c */
void _assert_fail(const char *, const char *, const char *)
    __attribute__((noreturn));
void _abort(const char *, const char *, const char *, void *)
    __attribute__((noreturn));

#define PANIC(s, r)                            \
    do {                                       \
        _abort(__FILE__, STR(__LINE__), s, r); \
    } while (0)

#define assert(e)                                      \
    do {                                               \
        if (!(e))                                      \
            _assert_fail(__FILE__, STR(__LINE__), #e); \
    } while (0)

/* cpu_<architecture>.c: low-level CPU functions */
void cpu_init(void);
void cpu_halt(void) __attribute__((noreturn));
void cpu_intr_enable(void);
void cpu_intr_disable(void);
extern int cpu_intr_depth;

/* CPU trap and interrupt vectors are defined in cpu_vectors_<architecture>.S */

/* intr.c: interrupt handling */
void intr_register_irq(unsigned irq, int (*handler)(void *), void *arg);
void intr_irq_handler(uint64_t irq);

/* mem.c: low-level page alloc routines */
void mem_init(void);
void *mem_ialloc_pages(size_t num);
void mem_lock_heap(uintptr_t *start, size_t *size);

/* lib.c: minimal bits of stdc we need */
void *memset(void *dest, int c, size_t n);
void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *vl, const void *vr, size_t n);
int strcmp(const char *l, const char *r);
int strncmp(const char *l, const char *r, size_t n);
char *strcpy(char *restrict dest, const char *restrict src);
size_t strlen(const char *s);
int isspace(int c);

/* snprintf/vsnprint live in printf.c / printf.h */
#include "printf.h"

/* platform.c: specifics for hvt or virito platform */
void platform_init(void *arg, uint64_t *tls_base);
const char *platform_cmdline(void);
uint64_t platform_mem_size(void);
void platform_exit(int status, void *cookie) __attribute__((noreturn));
int platform_puts(const char *buf, int n);
int platform_set_tls_base(uint64_t base);

/* platform_intr.c: platform-specific interrupt handling */
void platform_intr_init(void);
void platform_intr_clear_irq(unsigned irq);
void platform_intr_mask_irq(unsigned irq);
void platform_intr_ack_irq(unsigned irq);

/* cmdline.c: command line parsing */
char *cmdline_parse(const char *cmdline);

/* log.c: */
typedef enum {
    ERROR=0,
    WARN, 
    INFO, 
    DEBUG,
} log_level_t;
int log(log_level_t level, const char *fmt, ...)
	__attribute__ ((format (printf, 2, 3)));
void log_set_level(log_level_t level);

/* compiler-only memory "barrier" */
#define cc_barrier() __asm__ __volatile__("" : : : "memory")

#define NSEC_PER_SEC	1000000000ULL

#endif /* __BINDINGS_H__ */
