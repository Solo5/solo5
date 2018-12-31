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

#include "bindings.h"

/*
 * TODO: Argument types need a re-check against the kernel types.
 * We are most certainly missing clobbers in the syscall functions.
 */

#define SYS_read          63
#define SYS_write         64
#define SYS_pread64       67
#define SYS_pwrite64      68
#define SYS_clock_gettime 113
#define SYS_exit_group    94
#define SYS_ppoll         73

int sys_read(int fd, char *buf, size_t size)
{
    register long x8 __asm__("x8") = SYS_read;
    register long x0 __asm__("x0") = fd;
    register long x1 __asm__("x1") = (long)buf;
    register long x2 __asm__("x2") = (long)size;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (x0)
            : "r" (x8), "r" (x0), "r" (x1), "r" (x2)
            : "memory", "cc"
    );

    return (int)x0;
}

int sys_write(int fd, const char *buf, size_t size)
{
    register long x8 __asm__("x8") = SYS_write;
    register long x0 __asm__("x0") = fd;
    register long x1 __asm__("x1") = (long)buf;
    register long x2 __asm__("x2") = (long)size;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (x0)
            : "r" (x8), "r" (x0), "r" (x1), "r" (x2)
            : "memory", "cc"
    );

    return (int)x0;
}

int sys_pread64(int fd, char *buf, size_t size, long long pos)
{
    register long x8 __asm__("x8") = SYS_pread64;
    register long x0 __asm__("x0") = fd;
    register long x1 __asm__("x1") = (long)buf;
    register long x2 __asm__("x2") = (long)size;
    register long x3 __asm__("x3") = (long)pos;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (x0)
            : "r" (x8), "r" (x0), "r" (x1), "r" (x2), "r" (x3)
            : "memory", "cc"
    );

    return (int)x0;
}

int sys_pwrite64(int fd, const char *buf, size_t size, long long pos)
{
    register long x8 __asm__("x8") = SYS_pwrite64;
    register long x0 __asm__("x0") = fd;
    register long x1 __asm__("x1") = (long)buf;
    register long x2 __asm__("x2") = (long)size;
    register long x3 __asm__("x3") = (long)pos;

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (x0)
            : "r" (x8), "r" (x0), "r" (x1), "r" (x2), "r" (x3)
            : "memory", "cc"
    );

    return (int)x0;
}

void sys_exit_group(int status)
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

int sys_clock_gettime(const int which, struct sys_timespec *ts)
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

    return (int)x0;
}

int sys_ppoll(struct sys_pollfd *fds, unsigned int nfds,
        struct sys_timespec *ts)
{
    register long x8 __asm__("x8") = SYS_ppoll;
    register long x0 __asm__("x0") = (long)fds;
    register long x1 __asm__("x1") = (long)nfds;
    register long x2 __asm__("x2") = (long)ts;
    register long x3 __asm__("x3") = 0; /* sigmask */
    register long x4 __asm__("x4") = 0; /* sigsetsize */

    __asm__ __volatile__ (
            "svc 0"
            : "=r" (x0)
            : "r" (x8), "r" (x0), "r" (x1), "r" (x2), "r" (x3), "r" (x4)
            : "memory", "cc"
    );

    return (int)x0;
}
