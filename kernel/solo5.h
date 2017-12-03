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

/*
 * Solo5 public APIs.
 */

/*
 * Application entry point.
 *
 * FIXME: These APIs are to be considered unstable and have multiple known
 * issues (type confusion, missing const-ness, in/out parameters, inconsistent
 * return values). The comments in this file document the current
 * implementation.
 */
int solo5_app_main(char *cmdline);

/* 
 * Platform information.  Currently only memory info.
 */
struct solo5_info {
    char *cmdline;
    uint64_t heap_start;
    uint64_t heap_end;
};
void solo5_get_info(struct solo5_info *info);

/*
 * Network I/O.
 */

/*
 * Send a single network packet (*data) of size (n). Returns -1 on failure, 0
 * on success.
 */
int solo5_net_write_sync(uint8_t *data, int n);

/*
 * Receive a single network packet into (*data) up to a maximum size of (*n).
 * Returns 0 on success and size of received packet (*n), -1 on failure or if
 * no packets are available.
 */
int solo5_net_read_sync(uint8_t *data, int *n);

/*
 * Returns a pointer to the network MAC address, formatted as a C string
 * XX:XX:XX:XX:XX:XX.
 */
char *solo5_net_mac_str(void);

/*
 * Block I/O.
 */

/*
 * Writes (n) bytes from (*data) starting at the sector (sec) to the block
 * device. (n) does not need to be a multiple of the sector size. Returns 0 on
 * success, -1 on error.
 */
int solo5_blk_write_sync(uint64_t sec, uint8_t *data, int n);

/*
 * Reads (*n) bytes starting at the sector (sec) from the block device into
 * (*data). (n) does not need to be a multiple of the sector size. Returns 0 on
 * success, -1 on error.
 *
 * XXX: (*n) is not set as an output parameter. ukvm assert()s that exactly
 * (*n) bytes were written to the device.
 */
int solo5_blk_read_sync(uint64_t sec, uint8_t *data, int *n);

/*
 * Returns the block device sector size.
 */
int solo5_blk_sector_size(void);

/*
 * Returns the size of the block device in sectors.
 */
uint64_t solo5_blk_sectors(void);

/*
 * Returns true if the block device is writable.
 */
int solo5_blk_rw(void);

/*
 * Console I/O.
 */

/*
 * Writes (n) bytes from (*buf) to the console. Returns number of bytes
 * actually written.
 */
int solo5_console_write(const char *buf, size_t n);

/*
 * Exits the application.
 */
void solo5_exit(void) __attribute__((noreturn));

/*
 * Time.
 */

/*
 * Returns monotonic time in nanoseconds since an unspecified period in the
 * past.
 */
uint64_t solo5_clock_monotonic(void);

/*
 * Returns wall clock time in nanoseconds since the UNIX epoch.
 */
uint64_t solo5_clock_wall(void);

/*
 * Blocks until monotonic time reaches until_nsecs or I/O is possible,
 * whichever is sooner. Returns 1 if I/O is possible, otherwise 0.
 *
 * TODO: Extend this interface to select which I/O events are of interest.
 */
int solo5_poll(uint64_t until_nsecs);

#endif
