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

#include "spt.h"

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
    if (capacity < 512)
        errx(1, "%s: Backing storage must be at least 1 block (512 bytes) "
                "in size", diskfile);

    spt->bi->blocki.present = 1;
    spt->bi->blocki.block_size = 512;
    spt->bi->blocki.capacity = capacity;
    spt->bi->blocki.hostfd = diskfd;

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
