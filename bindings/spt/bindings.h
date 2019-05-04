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
 * bindings.h: Solo5 bindings, spt implementation additions.
 *
 * This header file includes (supersedes) the common bindings.h for the spt
 * implementation.
 */

#ifndef __SPT_BINDINGS_H__
#define __SPT_BINDINGS_H__

#include "../bindings.h"
#include "spt_abi.h"

#define SYS_STDOUT 1

long sys_read(long fd, void *buf, long size);
long sys_write(long fd, const void *buf, long size);
long sys_pread64(long fd, void *buf, long size, long pos);
long sys_pwrite64(long fd, const void *buf, long size, long pos);

void sys_exit_group(long status) __attribute__((noreturn));

struct sys_timespec {
    uint64_t tv_sec;
    long tv_nsec;
};

#define SYS_CLOCK_REALTIME 0
#define SYS_CLOCK_MONOTONIC 1

long sys_clock_gettime(const long which, void *ts);

struct sys_pollfd {
    int fd;
    short events;
    short revents;
};

#define SYS_POLLIN 1
#define SYS_EINTR -4
#define SYS_EAGAIN -11

long sys_ppoll(void *fds, long nfds, void *ts);

#define SYS_ARCH_SET_FS		0x1002

long sys_arch_prctl(long code, long addr);

void block_init(struct spt_boot_info *arg);
void net_init(struct spt_boot_info *arg);

int platform_set_tls_base(uint64_t base);
void platform_init_tls(void);

#endif /* __SPT_BINDINGS_H__ */
