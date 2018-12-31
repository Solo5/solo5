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
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <seccomp.h>

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

    /* set up virtual disk */
    diskfd = open(diskfile, O_RDWR);
    if (diskfd == -1)
        err(1, "Could not open disk: %s", diskfile);

    spt->bi->blocki.present = 1;
    spt->bi->blocki.block_size = 512;
    spt->bi->blocki.capacity = lseek(diskfd, 0, SEEK_END);
    spt->bi->blocki.hostfd = diskfd;

    int rc = -1;

    rc = seccomp_rule_add(spt->sc_ctx, SCMP_ACT_ALLOW, SCMP_SYS(pread64), 1,
            SCMP_A0(SCMP_CMP_EQ, diskfd));
    if (rc != 0)
        errx(1, "seccomp_rule_add(pread64, fd=%d) failed: %s", diskfd,
                strerror(-rc));
    rc = seccomp_rule_add(spt->sc_ctx, SCMP_ACT_ALLOW, SCMP_SYS(pwrite64), 1,
            SCMP_A0(SCMP_CMP_EQ, diskfd));
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
