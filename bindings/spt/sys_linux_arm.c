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

#define SYS_read 3
#define SYS_write 4
#define SYS_pread64 180
#define SYS_pwrite64 181
#define SYS_clock_gettime 263
#define SYS_exit_group 248
#define SYS_epoll_pwait 346
#define SYS_timerfd_settime 353

long sys_read(long fd, void *buf, long size)
{
    register long r7 __asm__("r7") = SYS_read;
    register long r0 __asm__("r0") = fd;
    register long r1 __asm__("r1") = (long)buf;
    register long r2 __asm__("r2") = size;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (r0)
            : "r" (r7), "r" (r0), "r" (r1), "r" (r2)
            : "memory", "cc"
    );

    return r0;
}

long sys_write(long fd, const void *buf, long size)
{
    register long r7 __asm__("r7") = SYS_write;
    register long r0 __asm__("r0") = fd;
    register long r1 __asm__("r1") = (long)buf;
    register long r2 __asm__("r2") = size;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (r0)
            : "r" (r7), "r" (r0), "r" (r1), "r" (r2)
            : "memory", "cc"
    );

    return r0;
}

long sys_pread64(long fd, void *buf, long size, solo5_off_t pos)
{
    register long r7 __asm__("r7") = SYS_pread64;
    register long r0 __asm__("r0") = fd;
    register long r1 __asm__("r1") = (long)buf;
    register long r2 __asm__("r2") = size;
    register long r3 __asm__("r3") = 0;
    register long r4 __asm__("r4") = (long)(pos & 0xffffffff);
    register long r5 __asm__("r5") = (long)(pos >> 32);

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (r0)
            : "r" (r7), "r" (r0), "r" (r1), "r" (r2), "r" (r3), "r" (r4), "r" (r5)
            : "memory", "cc"
    );

    return r0;
}

long sys_pwrite64(long fd, const void *buf, long size, solo5_off_t pos)
{
    register long r7 __asm__("r7") = SYS_pwrite64;
    register long r0 __asm__("r0") = fd;
    register long r1 __asm__("r1") = (long)buf;
    register long r2 __asm__("r2") = size;
    register long r3 __asm__("r3") = 0;
    register long r4 __asm__("r4") = (long)(pos & 0xffffffff);
    register long r5 __asm__("r5") = (long)(pos >> 32);

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (r0)
            : "r" (r7), "r" (r0), "r" (r1), "r" (r2), "r" (r3), "r" (r4), "r" (r5)
            : "memory", "cc"
    );

    return r0;
}

void sys_exit_group(long status)
{
    register long r7 __asm__("r7") = SYS_exit_group;
    register long r0 __asm__("r0") = status;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (r0)
            : "r" (r7), "r" (r0)
            : "memory", "cc"
    );

    for(;;);
}

long sys_clock_gettime(const long which, void *ts)
{
    register long r7 __asm__("r7") = SYS_clock_gettime;
    register long r0 __asm__("r0") = which;
    register long r1 __asm__("r1") = (long)ts;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (r0)
            : "r" (r7), "r" (r0), "r" (r1)
            : "memory", "cc"
    );

    return r0;
}

long sys_epoll_pwait(long epfd, void *events, long maxevents, long timeout,
        void *sigmask, long sigsetsize)
{
    register long r7 __asm__("r7") = SYS_epoll_pwait;
    register long r0 __asm__("r0") = epfd;
    register long r1 __asm__("r1") = (long)events;
    register long r2 __asm__("r2") = maxevents;
    register long r3 __asm__("r3") = timeout;
    register long r4 __asm__("r4") = (long)sigmask;
    register long r5 __asm__("r5") = sigsetsize;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (r0)
            : "r" (r7), "r" (r0), "r" (r1), "r" (r2), "r" (r3), "r" (r4),
              "r" (r5)
            : "memory", "cc"
    );

    return r0;
}

long sys_timerfd_settime(long fd, long flags, const void *utmr, void *otmr)
{
    register long r7 __asm__("r7") = SYS_timerfd_settime;
    register long r0 __asm__("r0") = fd;
    register long r1 __asm__("r1") = flags;
    register long r2 __asm__("r2") = (long)utmr;
    register long r3 __asm__("r3") = (long)otmr;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (r0)
            : "r" (r7), "r" (r0), "r" (r1), "r" (r2), "r" (r3)
            : "memory", "cc"
    );

    return r0;
}
