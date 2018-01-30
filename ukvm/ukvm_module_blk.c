/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of ukvm, a unikernel monitor.
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
 * ukvm_module_blk.c: Block device module.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ukvm.h"

static struct ukvm_blkinfo blkinfo;
static char *diskfile;
static int diskfd;

static void hypercall_blkinfo(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    struct ukvm_blkinfo *info =
        UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_blkinfo));

    info->sector_size = blkinfo.sector_size;
    info->num_sectors = blkinfo.num_sectors;
    info->rw = blkinfo.rw;
}

static void hypercall_blkwrite(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    struct ukvm_blkwrite *wr =
        UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_blkwrite));
    ssize_t ret;
    off_t pos, end;

#if 0
    assert(wr->len <= SSIZE_MAX);
#else
    /*
     * XXX: Artificially limit to single sector writes for now.
     */
    if (wr->len != blkinfo.sector_size) {
        wr->ret = -1;
        return;
    }
#endif
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

    ret = pwrite(diskfd, UKVM_CHECKED_GPA_P(hv, wr->data, wr->len), wr->len,
            pos);
#if 0
    assert(ret == wr->len);
    wr->ret = 0;
#else
    if (ret == wr->len)
        wr->ret = 0;
    else
        wr->ret = -1;
#endif
}

static void hypercall_blkread(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    struct ukvm_blkread *rd =
        UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_blkread));
    ssize_t ret;
    off_t pos, end;

#if 0
    assert(rd->len <= SSIZE_MAX);
#else
    /*
     * XXX: Artificially limit to single sector writes for now.
     */
    if (rd->len != blkinfo.sector_size) {
        rd->ret = -1;
        return;
    }
#endif
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

    ret = pread(diskfd, UKVM_CHECKED_GPA_P(hv, rd->data, rd->len), rd->len,
            pos);
#if 0
    assert(ret == rd->len);
    rd->ret = 0;
#else
    if (ret == rd->len)
        rd->ret = 0;
    else
        rd->ret = -1;
#endif
}

static int handle_cmdarg(char *cmdarg)
{
    if (strncmp("--disk=", cmdarg, 7))
        return -1;
    diskfile = cmdarg + 7;

    return 0;
}

static int setup(struct ukvm_hv *hv)
{
    if (diskfile == NULL)
        return -1;

    /* set up virtual disk */
    diskfd = open(diskfile, O_RDWR);
    if (diskfd == -1)
        err(1, "Could not open disk: %s", diskfile);

    blkinfo.sector_size = 512;
    blkinfo.num_sectors = lseek(diskfd, 0, SEEK_END) / 512;
    blkinfo.rw = 1;

    assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_BLKINFO,
                hypercall_blkinfo) == 0);
    assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_BLKWRITE,
                hypercall_blkwrite) == 0);
    assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_BLKREAD,
                hypercall_blkread) == 0);

    return 0;
}

static char *usage(void)
{
    return "--disk=IMAGE (file exposed to the unikernel as a raw block device)";
}

struct ukvm_module ukvm_module_blk = {
    .name = "blk",
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage
};
