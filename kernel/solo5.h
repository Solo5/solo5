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
 * The value returned from this function is the application's exit status.
 *
 * FIXME: These APIs are to be considered unstable.  The comments in this file
 * document the current implementation.
 */
int solo5_app_main(const char *cmdline);

/*
 * Network I/O.
 *
 * Currently only a single network device is supported. This device mimics an
 * Ethernet v2 network interface with an MTU of 1500 bytes. Therefore, buffers
 * passed to solo5_net_read_sync() must be at least 1514 bytes in length
 * (including space for the Ethernet data link layer).
 */

/*
 * Transmit a single network packet of (size) bytes from (*buf). (size) must be
 * less than or equal to 1514 bytes.
 *
 * On failure:
 *     Returns -1. This includes transient error conditions (e.g. short write)
 *     and invalid arguments (e.g. size too big).
 * On success:
 *     Returns 0.
 */
int solo5_net_write_sync(const uint8_t *buf, size_t size);

/*
 * Receive a single network packet into (*buf), up to a maximum of (buf_size)
 * bytes. (buf_size) must be at least 1514 bytes.
 *
 * On failure:
 *     Returns -1. This includes invalid argument errors (e.g. buf_size too
 *     small).
 * If no packets are available:
 *     Returns 0 and a (*read_size) of 0.
 * On success:
 *     Returns 0 and the length of the received packet in bytes in (*read_size).
 */
int solo5_net_read_sync(uint8_t *restrict buf, size_t buf_size, size_t
        *restrict read_size);

/*
 * Returns a pointer to the network MAC address, formatted as a C string
 * XX:XX:XX:XX:XX:XX.
 */
const char *solo5_net_mac_str(void);

/*
 * Block I/O.
 *
 * Currently only a single sector-addressable block device is supported, and
 * only single-sector reads and writes are supported.
 */

/*
 * Writes (size) bytes from (*buf) at the sector (offset) to the block device.
 * (size) must be equal to the sector size.
 *
 * On failure:
 *     Returns -1. This includes invalid argument errors and incomplete writes.
 * On success:
 *     Returns 0.
 */
int solo5_blk_write_sync(uint64_t offset, const uint8_t *buf, size_t size);

/*
 * Reads (size) bytes at the sector (offset) from the block device into (*buf).
 * (size) must be equal to the sector size.
 *
 * On failure:
 *     Returns -1. This includes invalid argument errors and incomplete reads.
 * On success:
 *     Returns 0.
 */
int solo5_blk_read_sync(uint64_t offset, uint8_t *buf, size_t size);

/*
 * Returns the block device sector size in bytes.
 */
int solo5_blk_sector_size(void);

/*
 * Returns the capacity of the block device in sectors.
 */
uint64_t solo5_blk_capacity(void);

/*
 * Returns 1 if the block device is writable, otherwise 0.
 */
int solo5_blk_rw(void);

/*
 * Console I/O.
 */

/*
 * Writes (size) characters from (*buf) to the console.
 *
 * Always succeeds, however the implementation may truncate messages (e.g. due
 * to resource exhaustion on the host).
 */
void solo5_console_write(const char *buf, size_t size);

/*
 * Immediately exits the application, with an exit code of (status).
 */
void solo5_exit(int status) __attribute__((noreturn));

/*
 * Memory allocation. These APIs correspond to their standard C equivalents.
 */
void *solo5_malloc(size_t);
void solo5_free(void *);
void *solo5_calloc(size_t, size_t);
void *solo5_realloc(void *, size_t);

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
 * Blocks until monotonic time reaches until_nsecs or network I/O is possible,
 * whichever is sooner.
 *
 * If network I/O is possible:
 *     Returns 1.
 * If monotonic time has reached until_nsecs and network I/O is not possible:
 *     Returns 0.
 *
 * TODO: Extend this interface to select which I/O events are of interest.
 */
int solo5_poll(uint64_t until_nsecs);

#endif
