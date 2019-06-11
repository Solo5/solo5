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

#include "bindings.h"

/*
 * The sys_ functions in this file are intentionally weakly typed as they only
 * pass through values to/from the system call without interpretation. All
 * integer values are passed as (long) and all pointer values are passed as
 * (void *).
 *
 * TODO: This will need to be re-considered for 32-bit archs, and we should
 * also consider explicitly inlining these functions.
 */

#define SYS_read          0
#define SYS_write         1
#define SYS_pread64       17
#define SYS_pwrite64      18
#define SYS_arch_prctl    158
#define SYS_clock_gettime 228
#define SYS_exit_group    231
#define SYS_ppoll         271
#define SYS_epoll_wait    232

long sys_read(long fd, void *buf, long size)
{
    long ret;

    __asm__ __volatile__ (
            "syscall"
            : "=a" (ret)
            : "a" (SYS_read), "D" (fd), "S" (buf), "d" (size)
            : "rcx", "r11", "memory"
    );

    return ret;
}

long sys_write(long fd, const void *buf, long size)
{
    long ret;

    __asm__ __volatile__ (
            "syscall"
            : "=a" (ret)
            : "a" (SYS_write), "D" (fd), "S" (buf), "d" (size)
            : "rcx", "r11", "memory"
    );

    return ret;
}

long sys_pread64(long fd, void *buf, long size, long pos)
{
    long ret;
    register long r10 asm("r10") = pos;

    __asm__ __volatile__ (
            "syscall"
            : "=a" (ret)
            : "a" (SYS_pread64), "D" (fd), "S" (buf), "d" (size), "r" (r10)
            : "rcx", "r11", "memory"
    );

    return ret;
}

long sys_pwrite64(long fd, const void *buf, long size, long pos)
{
    long ret;
    register long r10 asm("r10") = pos;

    __asm__ __volatile__ (
            "syscall"
            : "=a" (ret)
            : "a" (SYS_pwrite64), "D" (fd), "S" (buf), "d" (size), "r" (r10)
            : "rcx", "r11", "memory"
    );

    return ret;
}

void sys_exit_group(long status)
{
    __asm__ __volatile__ (
            "syscall"
            :
            : "a" (SYS_exit_group), "D" (status)
            : "rcx", "r11", "memory"
    );

    for(;;);
}

long sys_clock_gettime(const long which, void *ts)
{
    int ret;

    __asm__ __volatile__ (
            "syscall"
            : "=a" (ret)
            : "a" (SYS_clock_gettime), "D" (which), "S" (ts)
            : "rcx", "r11", "memory"
    );

    return ret;
}

long sys_ppoll(void *fds, long nfds, void *ts)
{
    int ret;
    register int64_t r10 asm("r10") = 0;
    register int64_t r8 asm("r8") = 0;

    __asm__ __volatile__ (
            "syscall"
            : "=a" (ret)
            : "a" (SYS_ppoll), "D" (fds), "S" (nfds), "d" (ts),
              "r" (r10) /* sigmask */, "r" (r8) /* sigsetsize */
            : "rcx", "r11", "memory"
    );

    return ret;
}

long sys_epoll_wait(long epfd, void *events, long maxevents, long timeout)
{
    long ret;
    register long r10 asm("r10") = timeout;

    __asm__ __volatile__ (
            "syscall"
            : "=a" (ret)
            : "a" (SYS_epoll_wait), "D" (epfd), "S" (events), "d" (maxevents),
              "r" (r10)
            : "rcx", "r11", "memory"
    );

    return ret;
}

long sys_arch_prctl(long code, long addr)
{
    long ret;

    __asm__ __volatile__ (
            "syscall"
            : "=a" (ret)
            : "a" (SYS_arch_prctl), "D" (code), "S" (addr)
            : "rcx", "r11", "memory"
    );

    return ret;
}
