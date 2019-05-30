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

#define SYS_read 63
#define SYS_write 64
#define SYS_pread64 67
#define SYS_pwrite64 68
#define SYS_clock_gettime 113
#define SYS_exit_group 94
#define SYS_epoll_pwait 22
#define SYS_timerfd_settime 86

long sys_read(long fd, void *buf, long size)
{
    register long x8 __asm__("x8") = SYS_read;
    register long x0 __asm__("x0") = fd;
    register long x1 __asm__("x1") = (long)buf;
    register long x2 __asm__("x2") = size;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (x0)
            : "r" (x8), "r" (x0), "r" (x1), "r" (x2)
            : "memory", "cc"
    );

    return x0;
}

long sys_write(long fd, const void *buf, long size)
{
    register long x8 __asm__("x8") = SYS_write;
    register long x0 __asm__("x0") = fd;
    register long x1 __asm__("x1") = (long)buf;
    register long x2 __asm__("x2") = size;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (x0)
            : "r" (x8), "r" (x0), "r" (x1), "r" (x2)
            : "memory", "cc"
    );

    return x0;
}

long sys_pread64(long fd, void *buf, long size, long pos)
{
    register long x8 __asm__("x8") = SYS_pread64;
    register long x0 __asm__("x0") = fd;
    register long x1 __asm__("x1") = (long)buf;
    register long x2 __asm__("x2") = size;
    register long x3 __asm__("x3") = pos;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (x0)
            : "r" (x8), "r" (x0), "r" (x1), "r" (x2), "r" (x3)
            : "memory", "cc"
    );

    return x0;
}

long sys_pwrite64(long fd, const void *buf, long size, long pos)
{
    register long x8 __asm__("x8") = SYS_pwrite64;
    register long x0 __asm__("x0") = fd;
    register long x1 __asm__("x1") = (long)buf;
    register long x2 __asm__("x2") = size;
    register long x3 __asm__("x3") = pos;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (x0)
            : "r" (x8), "r" (x0), "r" (x1), "r" (x2), "r" (x3)
            : "memory", "cc"
    );

    return x0;
}

void sys_exit_group(long status)
{
    register long x8 __asm__("x8") = SYS_exit_group;
    register long x0 __asm__("x0") = status;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (x0)
            : "r" (x8), "r" (x0)
            : "memory", "cc"
    );

    for(;;);
}

long sys_clock_gettime(const long which, void *ts)
{
    register long x8 __asm__("x8") = SYS_clock_gettime;
    register long x0 __asm__("x0") = which;
    register long x1 __asm__("x1") = (long)ts;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (x0)
            : "r" (x8), "r" (x0), "r" (x1)
            : "memory", "cc"
    );

    return x0;
}

long sys_epoll_pwait(long epfd, void *events, long maxevents, long timeout,
        void *sigmask, long sigsetsize)
{
    register long x8 __asm__("x8") = SYS_epoll_pwait;
    register long x0 __asm__("x0") = epfd;
    register long x1 __asm__("x1") = (long)events;
    register long x2 __asm__("x2") = maxevents;
    register long x3 __asm__("x3") = timeout;
    register long x4 __asm__("x4") = (long)sigmask;
    register long x5 __asm__("x5") = sigsetsize;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (x0)
            : "r" (x8), "r" (x0), "r" (x1), "r" (x2), "r" (x3), "r" (x4),
              "r" (x5)
            : "memory", "cc"
    );

    return x0;
}

long sys_timerfd_settime(long fd, long flags, const void *utmr, void *otmr)
{
    register long x8 __asm__("x8") = SYS_timerfd_settime;
    register long x0 __asm__("x0") = fd;
    register long x1 __asm__("x1") = flags;
    register long x2 __asm__("x2") = (long)utmr;
    register long x3 __asm__("x3") = (long)otmr;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (x0)
            : "r" (x8), "r" (x0), "r" (x1), "r" (x2), "r" (x3)
            : "memory", "cc"
    );

    return x0;
}
