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

#define _GNU_SOURCE
#include <err.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <limits.h>

#include "ukvm-private.h"
#include "ukvm-modules.h"
#include "ukvm.h"

static struct ukvm_blkinfo blkinfo;
static char *diskfile;
static int diskfd;

static void ukvm_port_blkinfo(uint8_t *mem, uint64_t paddr)
{
    GUEST_CHECK_PADDR(paddr, GUEST_SIZE, sizeof (struct ukvm_blkinfo));
    struct ukvm_blkinfo *info = (struct ukvm_blkinfo *)(mem + paddr);

    info->sector_size = blkinfo.sector_size;
    info->num_sectors = blkinfo.num_sectors;
    info->rw = blkinfo.rw;
}

static void ukvm_port_blkwrite(uint8_t *mem, uint64_t paddr)
{
    GUEST_CHECK_PADDR(paddr, GUEST_SIZE, sizeof (struct ukvm_blkwrite));
    struct ukvm_blkwrite *wr = (struct ukvm_blkwrite *)(mem + paddr);
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

    GUEST_CHECK_PADDR(wr->data, GUEST_SIZE, wr->len);
    ret = pwrite(diskfd, mem + wr->data, wr->len, pos);
    assert(ret == wr->len);
    wr->ret = 0;
}

static void ukvm_port_blkread(uint8_t *mem, uint64_t paddr)
{
    GUEST_CHECK_PADDR(paddr, GUEST_SIZE, sizeof (struct ukvm_blkread));
    struct ukvm_blkread *rd = (struct ukvm_blkread *)(mem + paddr);
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

    GUEST_CHECK_PADDR(rd->data, GUEST_SIZE, rd->len);
    ret = pread(diskfd, mem + rd->data, rd->len, pos);
    assert(ret == rd->len);
    rd->ret = 0;
}

static int handle_exit(struct kvm_run *run, int vcpufd, uint8_t *mem)
{
    if ((run->exit_reason != KVM_EXIT_IO) ||
        (run->io.direction != KVM_EXIT_IO_OUT) ||
        (run->io.size != 4))
        return -1;

    uint64_t paddr =
        GUEST_PIO32_TO_PADDR((uint8_t *)run + run->io.data_offset);

    switch (run->io.port) {
    case UKVM_PORT_BLKINFO:
        ukvm_port_blkinfo(mem, paddr);
        break;
    case UKVM_PORT_BLKWRITE:
        ukvm_port_blkwrite(mem, paddr);
        break;
    case UKVM_PORT_BLKREAD:
        ukvm_port_blkread(mem, paddr);
        break;
    default:
        return -1;
    }

    return 0;
}

static int handle_cmdarg(char *cmdarg)
{
    if (strncmp("--disk=", cmdarg, 7))
        return -1;
    diskfile = cmdarg + 7;

    return 0;
}

static int setup(int vcpufd, uint8_t *mem)
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

    return 0;
}

static int get_fd(void)
{
    return 0; /* no fd for poll to sleep on (synchronous) */
}

static char *usage(void)
{
    return "--disk=IMAGE (file exposed to the unikernel as a raw block device)";
}

struct ukvm_module ukvm_blk = {
    .get_fd = get_fd,
    .handle_exit = handle_exit,
    .handle_cmdarg = handle_cmdarg,
    .setup = setup,
    .usage = usage,
    .name = "blk"
};

