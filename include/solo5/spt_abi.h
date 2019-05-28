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

/*
 * Block device information passed by the tender to the guest.
 */
struct spt_block_info {
    int present;                        /* Set to 1 if present/valid */
    uint64_t capacity;                  /* Capacity of block device, bytes */
    uint64_t block_size;                /* Minimum I/O unit, bytes */
    int hostfd;                         /* Host backing file descriptor */
};

/*
 * Network device information passed by the tender to the guest.
 */
struct spt_net_info {
    int present;                        /* Set to 1 if present/valid */
    uint8_t mac_address[6];             /* Guest MAC address */
    int hostfd;                         /* Host TAP file descriptor */
};

/*
 * A pointer to this structure is passed by the tender as the sole argument to
 * the guest entrypoint.
 */
struct spt_boot_info {
    uint64_t mem_size;                  /* Memory size in bytes */
    uint64_t kernel_end;                /* Address of end of kernel */
    const char * cmdline;               /* Address of command line (C string) */
    uint64_t tls_base;                  /* Address of TLS base */
    struct spt_block_info blocki;
    struct spt_net_info neti;
};

/*
 * The lowest memory address at which we can mmap() memory on the host. See
 * spt_main.c for an explanation.
 */
#define SPT_HOST_MEM_BASE 0x10000

/*
 * Guest low memory layout.
 */
#define SPT_BOOT_INFO_BASE (SPT_HOST_MEM_BASE + 0x1000)
#define SPT_CMDLINE_BASE   (SPT_HOST_MEM_BASE + 0x2000)

/*
 * Maximum size of guest command line, including the string terminator.
 */
#define SPT_CMDLINE_SIZE 8192

/*
 * TLS related #defines
 */
#if defined (__powerpc64__)
# define TLS_MIN_SIZE (32 * 1024)
# define TLS_NEGATIVE_EXTENT 0x8000
#endif

#endif /* SPT_ABI_H */
