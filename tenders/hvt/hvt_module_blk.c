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

#if HVT_FREEBSD_ENABLE_CAPSICUM
#include <sys/capsicum.h>
#endif

#include "../common/block_attach.h"
#include "hvt.h"
#include "solo5.h"

static bool module_in_use;
static struct mft *host_mft;

static void hypercall_block_write(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_hc_block_write *wr =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_hc_block_write));
    struct mft_entry *e = mft_get_by_index(host_mft, wr->handle,
            MFT_DEV_BLOCK_BASIC);
    if (e == NULL) {
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

    ret = pwrite(e->b.hostfd, HVT_CHECKED_GPA_P(hvt, wr->data, wr->len),
            wr->len, pos);
    assert(ret == wr->len);
    wr->ret = SOLO5_R_OK;
}

static void hypercall_block_read(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_hc_block_read *rd =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_hc_block_read));
    struct mft_entry *e = mft_get_by_index(host_mft, rd->handle,
            MFT_DEV_BLOCK_BASIC);
    if (e == NULL) {
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

    ret = pread(e->b.hostfd, HVT_CHECKED_GPA_P(hvt, rd->data, rd->len), rd->len,
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

static int setup(struct hvt *hvt, struct mft *mft)
{
    if (!module_in_use)
        return 0;

    host_mft = mft;
    assert(hvt_core_register_hypercall(HVT_HYPERCALL_BLOCK_WRITE,
                hypercall_block_write) == 0);
    assert(hvt_core_register_hypercall(HVT_HYPERCALL_BLOCK_READ,
                hypercall_block_read) == 0);

#if HVT_FREEBSD_ENABLE_CAPSICUM
    cap_rights_t rights;
    cap_rights_init(&rights, CAP_READ, CAP_WRITE, CAP_SEEK);
    if (cap_rights_limit(diskfd, &rights) == -1)
       err(1, "cap_rights_limit() failed");
#endif

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
