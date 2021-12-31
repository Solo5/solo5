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

#ifndef SOLO5_H_INCLUDED
#define SOLO5_H_INCLUDED

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Solo5 public APIs.
 *
 * These APIs are still under development and may change at any time.
 *
 * The documentation in this file reflects the *intent* of the API; current
 * Solo5 implementations may deviate from the specified behaviour in the short
 * term. If you find such cases that are not documented here, please file a
 * bug.
 */

/*
 * Error handling.
 *
 * For APIs returning a solo5_result_t:
 *
 *   Unless otherwise documented below, these APIs return only SOLO5_R_OK on
 *   success.
 *
 *   Application developers MUST NOT RELY on these APIs returning
 *   SOLO5_R_EINVAL or SOLO5_R_EUNSPEC. Solo5 implementations MAY choose to
 *   abort execution of the application in preference to returning an error
 *   result on failure.
 *
 * For APIs NOT returning a solo5_result_t:
 *
 *   These APIs either:
 *
 *   a) always succeed under normal operation, or
 *   b) abort execution of the application on failure.
 *
 * For the avoidance of doubt, calling APIs on devices that are not present
 * WILL result in aborting execution of the application.
 */
typedef enum {
    /*
     * The operation completed successfully.
     */
    SOLO5_R_OK = 0,
    /*
     * The operation cannot be completed at this time. Retrying an identical
     * operation at a later time may succeed.
     */
    SOLO5_R_AGAIN,
    /*
     * Invalid argument.
     */
    SOLO5_R_EINVAL,
    /*
     * The operation failed due to an unspecified error.
     */
    SOLO5_R_EUNSPEC
} solo5_result_t;

/*
 * Application entry point.
 *
 * The application is passed a pointer (*info) to a statically allocated
 * (struct solo5_start_info) with start of day information; any data in this
 * structure should be treated as read-only.
 *
 * The application receives the (info->heap_start, info->heap_size) of a
 * single, contiguous, non-executable, read/write memory region it may use for
 * heap or stack(s) as it sees fit.  At entry, the application is provided with
 * an initial stack growing down from (info->heap_start + info->heap_size).
 *
 * The application MUST NOT make any further assumptions about memory layout,
 * including where executable code or static data are located in memory.
 *
 * Returning from this function is equivalent to calling solo5_exit(<return
 * value>).
 */
struct solo5_start_info {
    const char *cmdline;
    uintptr_t heap_start;
    size_t heap_size;
};

int solo5_app_main(const struct solo5_start_info *info);

/*
 * Exit the application, returning (status) to the host if possible.
 *
 * Status values of 255 and above are reserved for use by Solo5.
 */
#define SOLO5_EXIT_SUCCESS	0
#define SOLO5_EXIT_FAILURE	1
#define SOLO5_EXIT_ABORT	255

void solo5_exit(int status) __attribute__((noreturn));

/*
 * Abort the application, returning a status of SOLO5_EXIT_ABORT to the host if
 * possible.
 *
 * This call is intended to signal an abnormal exit to the host. Depending on
 * the tender configuration, this may trigger further actions such a
 * generating a core file.
 */
void solo5_abort(void) __attribute__((noreturn));

/*
 * Set the architecture-specific TLS base register to (base).
 *
 * Solo5 implementations may return SOLO5_R_EINVAL if the (base) does not
 * satisfy architecture-specific requirements.
 */
solo5_result_t solo5_set_tls_base(uintptr_t base);

/*
 * Time.
 */

/*
 * Solo5 type for time values, with nanosecond precision.
 */
typedef uint64_t solo5_time_t;

/*
 * Returns monotonic time since an unspecified period in the past.
 */
solo5_time_t solo5_clock_monotonic(void);

/*
 * Returns wall clock time in UTC since the UNIX epoch (1970-01-01).
 */
solo5_time_t solo5_clock_wall(void);

/*
 * Type for I/O handles.
 */
typedef uint64_t solo5_handle_t;

/*
 * Type for sets of up to 64 I/O handles.
 */
typedef uint64_t solo5_handle_set_t;

/*
 * Suspends execution of the application until either:
 *
 *   a) monotonic time reaches (deadline), or
 *   b) at least one network device is ready for input.
 *
 * If (ready_set) is not NULL, it will be filled in with the set of
 * solo5_handle_t's ready for input.
 */
void solo5_yield(solo5_time_t deadline, solo5_handle_set_t *ready_set);

/*
 * Console I/O.
 */

/*
 * Writes (size) bytes from (*buf) to the console.
 *
 * Writing to the console is a best-effort operation: this operation will
 * always succeed but may lose data, for example in the event of resource
 * exhaustion on the host.
 */
void solo5_console_write(const char *buf, size_t size);

/*
 * Network I/O.
 */

/*
 * Ethernet address length in bytes.
 */
#define SOLO5_NET_ALEN          6
/*
 * Ethernet frame header (target, source, type) length in bytes.
 */
#define SOLO5_NET_HLEN          14

struct solo5_net_info {
    uint8_t mac_address[SOLO5_NET_ALEN];
    size_t mtu;                 /* Not including Ethernet header */
};

/*
 * Acquires a handle to the network device declared as (name) in the
 * application manifest. The returned handle is stored in (*handle), and
 * properties of the network device are stored in (*info). Caller must supply
 * space for struct solo5_net_info in (info). This function may only be called
 * once for each device (name). Subsequent calls will return SOLO5_R_EINVAL.
 */
solo5_result_t solo5_net_acquire(const char *name, solo5_handle_t *handle,
        struct solo5_net_info *info);

/*
 * Sends a single network packet to the network device identified by (handle),
 * from the buffer (*buf), without blocking.  If the packet cannot be sent due
 * to a transient error (e.g.  no resources available) it will be silently
 * dropped.
 *
 * The maximum allowed value for (size) is (solo5_net_info.mtu +
 * SOLO5_NET_HLEN). The packet must include the ethernet frame header.
 */
solo5_result_t solo5_net_write(solo5_handle_t handle, const uint8_t *buf,
        size_t size);

/*
 * Receives a single network packet from the network device identified by
 * (handle) into the buffer (*buf), without blocking.
 *
 * (size) must be at least (solo5_net_info.mtu + SOLO5_NET_HLEN).
 *
 * If no packets are available returns SOLO5_R_AGAIN, otherwise returns
 * SOLO5_R_OK and the size of the received packet including the ethernet frame
 * header in (*read_size).
 */
solo5_result_t solo5_net_read(solo5_handle_t handle, uint8_t *buf,
        size_t size, size_t *read_size);

/*
 * Block I/O.
 *
 * The minimum unit of I/O which can be performed on a block device is defined
 * by solo5_block_info.block_size. In practice we currently also limit the
 * *maximum* unit of I/O to a single block.
 *
 * These interfaces, depending on atomicity guarantees, may be extended in the
 * future to support I/O operations of >1 block per call.
 */

/*
 * Architecture-independent type for block device offsets, in bytes.
 */
typedef uint64_t solo5_off_t;

struct solo5_block_info {
    solo5_off_t capacity;       /* Capacity of block device, bytes */
    solo5_off_t block_size;     /* Minimum I/O unit (block size), bytes */
};

/*
 * Acquires a handle to the block device declared as (name) in the application
 * manifest. The returned handle is stored in (*handle), and properties of the
 * block device are stored in (*info). Caller must supply space for struct
 * solo5_block_info in (info). This function may only be called once for each
 * device (name). Subsequent calls will return SOLO5_R_EINVAL.
 */
solo5_result_t solo5_block_acquire(const char *name, solo5_handle_t *handle,
        struct solo5_block_info *info);

/*
 * Writes data of (size) bytes from the buffer (*buf) to the block device
 * identified by (handle), starting at byte (offset). Data is either written in
 * it's entirety or not at all ("short writes" are not possible).
 *
 * Both (size) and (offset) must be a multiple of the block size, otherwise
 * SOLO5_R_EINVAL is returned.
 *
 * NOTE: Current implementations further limit the *maximum* I/O size to a
 * single block.
 */
solo5_result_t solo5_block_write(solo5_handle_t handle, solo5_off_t offset,
        const uint8_t *buf, size_t size);

/*
 * Reads data of (size) bytes into the buffer (*buf) from the block device
 * identified by (handle), starting at byte (offset). Always reads the full
 * amount of (size) bytes ("short reads" are not possible).
 *
 * Both (size) and (offset) must be a multiple of the block size, otherwise
 * SOLO5_R_EINVAL is returned.
 *
 * NOTE: Current implementations further limit the *maximum* I/O size to a
 * single block.
 */
solo5_result_t solo5_block_read(solo5_handle_t handle, solo5_off_t offset,
        uint8_t *buf, size_t size);

#endif
