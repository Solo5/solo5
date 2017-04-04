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

#ifndef SOLO5_H_INCLUDED
#define SOLO5_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

/* Solo5 public APIs */

/* Application entry point */
int solo5_app_main(char *cmdline);

/* Network */
int solo5_net_write_sync(uint8_t *data, int n);
int solo5_net_read_sync(uint8_t *data, int *n);
char *solo5_net_mac_str(void);

/* Block */
int solo5_blk_write_sync(uint64_t sec, uint8_t *data, int n);
int solo5_blk_read_sync(uint64_t sec, uint8_t *data, int *n);
int solo5_blk_sector_size(void);
uint64_t solo5_blk_sectors(void);
int solo5_blk_rw(void);

/* Console */
int solo5_console_write(const char *buf, size_t n);

/* Exit */
void solo5_exit(void) __attribute__((noreturn));

/* Memory allocation */
void *solo5_malloc(size_t);
void solo5_free(void *);
void *solo5_calloc(size_t, size_t);
void *solo5_realloc(void *, size_t);

/* Time */
uint64_t solo5_clock_monotonic(void);
uint64_t solo5_clock_wall(void);

/* Sched related functions */
/* solo5_poll(): Block until monotonic time reaches until_nsecs or I/O is
 * possible, whichever is sooner. Returns 1 if I/O is possible, otherwise 0.
 *
 * TODO: Extend this interface to select which I/O events are of interest.
 */
int solo5_poll(uint64_t until_nsecs);


/* TODO: Replace lib stuff */
void *memset(void *dest, int c, size_t n);
void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *vl, const void *vr, size_t n);
int strcmp(const char *l, const char *r);
char *strcpy(char *restrict dest, const char *restrict src);
size_t strlen(const char *s);

#endif
