/*
 * Copyright (c) 2015-2018 Contributors as noted in the AUTHORS file
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
 * spt_module_block.c: Block device module.
 */

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <seccomp.h>

#include "spt.h"

#ifndef SPT_BLOCK_SIZE
#define SPT_BLOCK_SIZE 512
#endif

static char *diskfile;
static int diskfd = -1;

static int handle_cmdarg(char *cmdarg)
{
    if (strncmp("--disk=", cmdarg, 7))
        return -1;
    diskfile = cmdarg + 7;

    return 0;
}

static int setup(struct spt *spt)
{
    if (diskfile == NULL)
        return 0; /* not present */

    diskfd = open(diskfile, O_RDWR);
    if (diskfd == -1)
        err(1, "Could not open disk: %s", diskfile);
    off_t capacity = lseek(diskfd, 0, SEEK_END);
    if (capacity == -1)
        err(1, "%s: Could not determine capacity", diskfile);
    if (capacity < SPT_BLOCK_SIZE)
        errx(1, "%s: Backing storage must be at least 1 block (512 bytes) "
                "in size", diskfile);

    spt->bi->blocki.present = 1;
    spt->bi->blocki.block_size = SPT_BLOCK_SIZE;
    spt->bi->blocki.capacity = capacity;
    spt->bi->blocki.hostfd = diskfd;

    int rc = -1;

    /*
     * When reading or writing to the file descriptor, enforce that the
     * operation cannot be performed beyond the (detected) capacity, otherwise,
     * when backed by a regular file, the guest could grow the file size
     * arbitrarily.
     *
     * The Solo5 API mandates that reads/writes must be equal to block_size, so
     * we implement the above by ensuring that (A2 == block_size) && (A3 <=
     * (capacity - block_size) holds.
     */
    rc = seccomp_rule_add(spt->sc_ctx, SCMP_ACT_ALLOW, SCMP_SYS(pread64), 3,
            SCMP_A0(SCMP_CMP_EQ, diskfd),
            SCMP_A2(SCMP_CMP_EQ, spt->bi->blocki.block_size),
            SCMP_A3(SCMP_CMP_LE,
                (spt->bi->blocki.capacity - spt->bi->blocki.block_size)));
    if (rc != 0)
        errx(1, "seccomp_rule_add(pread64, fd=%d) failed: %s", diskfd,
                strerror(-rc));
    rc = seccomp_rule_add(spt->sc_ctx, SCMP_ACT_ALLOW, SCMP_SYS(pwrite64), 3,
            SCMP_A0(SCMP_CMP_EQ, diskfd),
            SCMP_A2(SCMP_CMP_EQ, spt->bi->blocki.block_size),
            SCMP_A3(SCMP_CMP_LE,
                (spt->bi->blocki.capacity - spt->bi->blocki.block_size)));
    if (rc != 0)
        errx(1, "seccomp_rule_add(pwrite64, fd=%d) failed: %s", diskfd,
                strerror(-rc));

    return 0;
}

static char *usage(void)
{
    return "--disk=IMAGE (file exposed to the unikernel as a raw block device)";
}

struct spt_module spt_module_block = {
    .name = "block",
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage
};
