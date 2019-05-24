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
 * block_attach.c: Common functions for attaching to block devices.
 */

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <err.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

/*
 * Attach to the block device specified by (path), returning its capacity in
 * bytes in (*capacity).
 */
int block_attach(const char *path, off_t *capacity_)
{
    int fd = open(path, O_RDWR);
    if (fd == -1)
        err(1, "Could not open block device: %s", path);
    off_t capacity = lseek(fd, 0, SEEK_END);
    if (capacity == -1)
        err(1, "%s: Could not determine capacity", path);
    if (capacity < 512)
        errx(1, "%s: Backing storage must be at least 1 block (512 bytes) "
                "in size", path);

    *capacity_ = capacity;
    return fd;
}
