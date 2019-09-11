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
 * spt_module_block.c: Block device module.
 */

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <assert.h>
#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <seccomp.h>

#include "../common/block_attach.h"
#include "spt.h"

static bool module_in_use;

static int handle_cmdarg(char *cmdarg, struct mft *mft)
{
    if (strncmp("--block:", cmdarg, 8) != 0)
        return -1;

    char name[MFT_NAME_SIZE];
    char path[PATH_MAX + 1];
    int rc = sscanf(cmdarg,
            "--block:%" XSTR(MFT_NAME_MAX) "[A-Za-z0-9]="
            "%" XSTR(PATH_MAX) "s", name, path);
    if (rc != 2)
        return -1;
    struct mft_entry *e = mft_get_by_name(mft, name, MFT_DEV_BLOCK_BASIC, NULL);
    if (e == NULL) {
        warnx("Resource not declared in manifest: '%s'", name);
        return -1;
    }

    off_t capacity;
    int fd = block_attach(path, &capacity);
    e->u.block_basic.capacity = capacity;
    e->u.block_basic.block_size = 512;
    e->b.hostfd = fd;
    e->attached = true;
    module_in_use = true;

    return 0;
}

static int setup(struct spt *spt, struct mft *mft)
{
    if (!module_in_use)
        return 0;

    for (unsigned i = 0; i != mft->entries; i++) {
        if (mft->e[i].type != MFT_DEV_BLOCK_BASIC || !mft->e[i].attached)
            continue;

        int rc = -1;

        /*
         * When reading or writing to the file descriptor, enforce that the
         * operation cannot be performed beyond the (detected) capacity,
         * otherwise, when backed by a regular file, the guest could grow the
         * file size arbitrarily.
         *
         * The Solo5 API mandates that reads/writes must be equal to
         * block_size, so we implement the above by ensuring that (A2 ==
         * block_size) && (A3 <= (capacity - block_size) holds.
         */
        rc = seccomp_rule_add(spt->sc_ctx, SCMP_ACT_ALLOW,
                SCMP_SYS(pread64), 3,
                SCMP_A0(SCMP_CMP_EQ, mft->e[i].b.hostfd),
                SCMP_A2(SCMP_CMP_EQ, mft->e[i].u.block_basic.block_size),
                SCMP_A3(SCMP_CMP_LE,
                    (mft->e[i].u.block_basic.capacity - mft->e[i].u.block_basic.block_size)));
        if (rc != 0)
            errx(1, "seccomp_rule_add(pread64, fd=%d) failed: %s",
                    mft->e[i].b.hostfd, strerror(-rc));
        rc = seccomp_rule_add(spt->sc_ctx, SCMP_ACT_ALLOW,
                SCMP_SYS(pwrite64), 3,
                SCMP_A0(SCMP_CMP_EQ, mft->e[i].b.hostfd),
                SCMP_A2(SCMP_CMP_EQ, mft->e[i].u.block_basic.block_size),
                SCMP_A3(SCMP_CMP_LE,
                    (mft->e[i].u.block_basic.capacity - mft->e[i].u.block_basic.block_size)));
        if (rc != 0)
            errx(1, "seccomp_rule_add(pwrite64, fd=%d) failed: %s",
                    mft->e[i].b.hostfd, strerror(-rc));
    }

    return 0;
}

static char *usage(void)
{
    return "--block:NAME=PATH (attach block device/file at PATH as block storage NAME)";
}

DECLARE_MODULE(block,
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage
)
