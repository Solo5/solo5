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
 * hvt_module_blk.c: Block device module.
 */

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <assert.h>
#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../common/block_attach.h"
#include "hvt.h"
#include "solo5.h"

static bool module_in_use;

static void hypercall_blkwrite(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_blkwrite *wr =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_blkwrite));
    struct mft_entry *e = mft_get_by_index(hvt->mft, wr->handle);
    if (e == NULL || e->type != MFT_BLOCK_BASIC) {
        wr->ret = SOLO5_R_EINVAL;
        return;
    }

    ssize_t ret;
    off_t pos, end;

    assert(wr->len <= SSIZE_MAX);
    if (wr->offset >= e->u.block_basic.capacity) {
        wr->ret = SOLO5_R_EINVAL;
        return;
    }
    pos = wr->offset;
    if (add_overflow(pos, wr->len, end)
            || (end > e->u.block_basic.capacity)) {
        wr->ret = SOLO5_R_EINVAL;
        return;
    }

    ret = pwrite(e->hostfd, HVT_CHECKED_GPA_P(hvt, wr->data, wr->len), wr->len,
            pos);
    assert(ret == wr->len);
    wr->ret = SOLO5_R_OK;
}

static void hypercall_blkread(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_blkread *rd =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_blkread));
    struct mft_entry *e = mft_get_by_index(hvt->mft, rd->handle);
    if (e == NULL || e->type != MFT_BLOCK_BASIC) {
        rd->ret = SOLO5_R_EINVAL;
        return;
    }

    ssize_t ret;
    off_t pos, end;

    assert(rd->len <= SSIZE_MAX);
    if (rd->offset >= e->u.block_basic.capacity) {
        rd->ret = SOLO5_R_EINVAL;
        return;
    }
    pos = rd->offset;
    if (add_overflow(pos, rd->len, end)
            || (end > e->u.block_basic.capacity)) {
        rd->ret = SOLO5_R_EINVAL;
        return;
    }

    ret = pread(e->hostfd, HVT_CHECKED_GPA_P(hvt, rd->data, rd->len), rd->len,
            pos);
    assert(ret == rd->len);
    rd->ret = SOLO5_R_OK;
}

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

    warnx("block_setup: rc=%d name=%s path=%s", rc, name, path);
    struct mft_entry *e = mft_get_by_name(mft, name, NULL);
    if (e == NULL || e->type != MFT_BLOCK_BASIC)
        return -1;
    off_t capacity;
    int fd = block_attach(path, &capacity);
    e->u.block_basic.capacity = capacity;
    e->u.block_basic.block_size = 512;
    e->hostfd = fd;
    e->ok = true;
    module_in_use = true;

    return 0;
}

static int setup(struct hvt *hvt)
{
    if (!module_in_use)
        return 0;

    assert(hvt_core_register_hypercall(HVT_HYPERCALL_BLKWRITE,
                hypercall_blkwrite) == 0);
    assert(hvt_core_register_hypercall(HVT_HYPERCALL_BLKREAD,
                hypercall_blkread) == 0);

    return 0;
}

static char *usage(void)
{
    return "--block:NAME=PATH (file exposed to the unikernel as a raw block device)";
}

BEGIN_REGISTER_MODULE(blk) {
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage
}
END_REGISTER_MODULE
