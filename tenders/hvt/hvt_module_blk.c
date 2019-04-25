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
 * hvt_module_blk.c: Block device module.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "hvt.h"

static struct hvt_blkinfo blkinfo;
static char *diskfile;
static int diskfd;

static void hypercall_blkinfo(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_blkinfo *info =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_blkinfo));

    info->sector_size = blkinfo.sector_size;
    info->num_sectors = blkinfo.num_sectors;
    info->rw = blkinfo.rw;
}

static void hypercall_blkwrite(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_blkwrite *wr =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_blkwrite));
    ssize_t ret;
    off_t pos, end;

    assert(wr->len <= SSIZE_MAX);
    if (wr->sector >= blkinfo.num_sectors) {
        wr->ret = -1;
        return;
    }
    pos = (off_t)blkinfo.sector_size * (off_t)wr->sector;
    if (add_overflow(pos, wr->len, end)
            || (end > blkinfo.num_sectors * blkinfo.sector_size)) {
        wr->ret = -1;
        return;
    }

    ret = pwrite(diskfd, HVT_CHECKED_GPA_P(hvt, wr->data, wr->len), wr->len,
            pos);
    assert(ret == wr->len);
    wr->ret = 0;
}

static void hypercall_blkdiscard(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_blkdiscard *di =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_blkdiscard));
    off_t pos, len, end;

    if (di->sector >= blkinfo.num_sectors) {
        di->ret = -1;
        return;
    }
    pos = (off_t)blkinfo.sector_size * (off_t)di->sector;
    len = (off_t)blkinfo.sector_size * (off_t)di->len_sectors;
    if (add_overflow(pos, len, end)
            || (end > blkinfo.num_sectors * blkinfo.sector_size))
    {
        di->ret = -1;
        return;
    }

#if defined(__linux__)
    ssize_t ret;
    ret = fallocate(
            diskfd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, pos, len);
    if (ret != 0)
    {
	    if (ret == -1 && errno == EOPNOTSUPP)
            di->ret = -2;
        else
            di->ret = -1;
    } else {
        di->ret = 0;
    }
#else
    di->ret = -2;
#endif
}

static void hypercall_blkread(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_blkread *rd =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_blkread));
    ssize_t ret;
    off_t pos, end;

    assert(rd->len <= SSIZE_MAX);
    if (rd->sector >= blkinfo.num_sectors) {
        rd->ret = -1;
        return;
    }
    pos = (off_t)blkinfo.sector_size * (off_t)rd->sector;
    if (add_overflow(pos, rd->len, end)
            || (end > blkinfo.num_sectors * blkinfo.sector_size)) {
        rd->ret = -1;
        return;
    }

    ret = pread(diskfd, HVT_CHECKED_GPA_P(hvt, rd->data, rd->len), rd->len,
            pos);
    assert(ret == rd->len);
    rd->ret = 0;
}

static int handle_cmdarg(char *cmdarg)
{
    if (strncmp("--disk=", cmdarg, 7))
        return -1;
    diskfile = cmdarg + 7;

    return 0;
}

static int setup(struct hvt *hvt)
{
    if (diskfile == NULL)
        return 0; /* Not present */

    /* set up virtual disk */
    diskfd = open(diskfile, O_RDWR);
    if (diskfd == -1)
        err(1, "Could not open disk: %s", diskfile);

    blkinfo.sector_size = 512;
    blkinfo.num_sectors = lseek(diskfd, 0, SEEK_END) / 512;
    blkinfo.rw = 1;

    assert(hvt_core_register_hypercall(HVT_HYPERCALL_BLKINFO,
                hypercall_blkinfo) == 0);
    assert(hvt_core_register_hypercall(HVT_HYPERCALL_BLKWRITE,
                hypercall_blkwrite) == 0);
    assert(hvt_core_register_hypercall(HVT_HYPERCALL_BLKREAD,
                hypercall_blkread) == 0);
    assert(hvt_core_register_hypercall(HVT_HYPERCALL_BLKDISCARD,
                hypercall_blkdiscard) == 0);

    return 0;
}

static char *usage(void)
{
    return "--disk=IMAGE (file exposed to the unikernel as a raw block device)";
}

BEGIN_REGISTER_MODULE(blk) {
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage
}
END_REGISTER_MODULE
