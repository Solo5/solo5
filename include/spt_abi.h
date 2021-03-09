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
 * spt_abi.h: spt guest/tender interface definitions.
 *
 * This header file must be kept self-contained with no external dependencies
 * other than C99 headers.
 */

#ifndef SPT_ABI_H
#define SPT_ABI_H

#include <stddef.h>
#include <stdint.h>
#include "elf_abi.h"

/*
 * ABI version. This must be incremented before cutting a release of Solo5 if
 * any material changes are made to the interfaces or data structures defined
 * in this file.
 */

#define SPT_ABI_VERSION 1

/*
 * Lowest virtual address at which guests can be loaded.
 */
#define SPT_GUEST_MIN_BASE 0x100000

/*
 * A pointer to this structure is passed by the tender as the sole argument to
 * the guest entrypoint.
 */
struct spt_boot_info {
    uint64_t mem_size;                  /* Memory size in bytes */
    uint64_t kernel_end;                /* Address of end of kernel */
    const char *cmdline;                /* Address of command line (C string) */
    const void *mft;                    /* Address of application manifest */
    int epollfd;                        /* epoll() set for yield() */
    int timerfd;                        /* internal timerfd for yield() */
};

/*
 * Identifier (data.u64) for internal timerfd in epoll() set.
 */
#define SPT_INTERNAL_TIMERFD (~1U)

/*
 * The lowest memory address at which we can mmap() memory on the host. See
 * spt_main.c for an explanation.
 */
#define SPT_HOST_MEM_BASE 0x10000

/*
 * Guest low memory layout.
 */
#define SPT_BOOT_INFO_BASE (SPT_HOST_MEM_BASE + 0x1000)

/*
 * Maximum size of guest command line, including the string terminator.
 */
#define SPT_CMDLINE_SIZE 8192

#endif /* SPT_ABI_H */
