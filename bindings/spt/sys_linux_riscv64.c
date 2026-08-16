/*
 * Copyright (c) 2015-2026 Contributors as noted in the AUTHORS file
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
 */

/*
 * riscv64 takes the generic syscall numbering from asm-generic/unistd.h, so
 * none of these match the ppc64le port they were written against. Errors come
 * back as a negative a0 rather than a condition bit, so there is nothing to
 * unpick afterwards.
 */
#define SYS_epoll_pwait     22
#define SYS_read            63
#define SYS_write           64
#define SYS_pread64         67
#define SYS_pwrite64        68
#define SYS_timerfd_settime 86
#define SYS_exit_group      94
#define SYS_clock_gettime   113

long sys_read(long fd, void *buf, long size)
{
    register long a7 __asm__("a7") = SYS_read;
    register long a0 __asm__("a0") = fd;
    register long a1 __asm__("a1") = (long)buf;
    register long a2 __asm__("a2") = size;

    __asm__ __volatile__("ecall"
                         : "+r"(a0)
                         : "r"(a7), "r"(a1), "r"(a2)
                         : "memory");

    return a0;
}

long sys_write(long fd, const void *buf, long size)
{
    register long a7 __asm__("a7") = SYS_write;
    register long a0 __asm__("a0") = fd;
    register long a1 __asm__("a1") = (long)buf;
    register long a2 __asm__("a2") = size;

    __asm__ __volatile__("ecall"
                         : "+r"(a0)
                         : "r"(a7), "r"(a1), "r"(a2)
                         : "memory");

    return a0;
}

long sys_pread64(long fd, void *buf, long size, long pos)
{
    register long a7 __asm__("a7") = SYS_pread64;
    register long a0 __asm__("a0") = fd;
    register long a1 __asm__("a1") = (long)buf;
    register long a2 __asm__("a2") = size;
    register long a3 __asm__("a3") = pos;

    __asm__ __volatile__("ecall"
                         : "+r"(a0)
                         : "r"(a7), "r"(a1), "r"(a2), "r"(a3)
                         : "memory");

    return a0;
}

long sys_pwrite64(long fd, const void *buf, long size, long pos)
{
    register long a7 __asm__("a7") = SYS_pwrite64;
    register long a0 __asm__("a0") = fd;
    register long a1 __asm__("a1") = (long)buf;
    register long a2 __asm__("a2") = size;
    register long a3 __asm__("a3") = pos;

    __asm__ __volatile__("ecall"
                         : "+r"(a0)
                         : "r"(a7), "r"(a1), "r"(a2), "r"(a3)
                         : "memory");

    return a0;
}

void sys_exit_group(long status)
{
    register long a7 __asm__("a7") = SYS_exit_group;
    register long a0 __asm__("a0") = status;

    __asm__ __volatile__("ecall" : : "r"(a7), "r"(a0) : "memory");

    for (;;)
        ;
}

long sys_clock_gettime(const long which, void *ts)
{
    register long a7 __asm__("a7") = SYS_clock_gettime;
    register long a0 __asm__("a0") = which;
    register long a1 __asm__("a1") = (long)ts;

    __asm__ __volatile__("ecall"
                         : "+r"(a0)
                         : "r"(a7), "r"(a1)
                         : "memory");

    return a0;
}

long sys_epoll_pwait(long epfd, void *events, long maxevents, long timeout,
                     void *sigmask, long sigsetsize)
{
    register long a7 __asm__("a7") = SYS_epoll_pwait;
    register long a0 __asm__("a0") = epfd;
    register long a1 __asm__("a1") = (long)events;
    register long a2 __asm__("a2") = maxevents;
    register long a3 __asm__("a3") = timeout;
    register long a4 __asm__("a4") = (long)sigmask;
    register long a5 __asm__("a5") = sigsetsize;

    __asm__ __volatile__("ecall"
                         : "+r"(a0)
                         : "r"(a7), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5)
                         : "memory");

    return a0;
}

long sys_timerfd_settime(long fd, long flags, const void *utmr, void *otmr)
{
    register long a7 __asm__("a7") = SYS_timerfd_settime;
    register long a0 __asm__("a0") = fd;
    register long a1 __asm__("a1") = flags;
    register long a2 __asm__("a2") = (long)utmr;
    register long a3 __asm__("a3") = (long)otmr;

    __asm__ __volatile__("ecall"
                         : "+r"(a0)
                         : "r"(a7), "r"(a1), "r"(a2), "r"(a3)
                         : "memory");

    return a0;
}
