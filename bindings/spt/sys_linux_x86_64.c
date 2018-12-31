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

#define SYS_read          0
#define SYS_write         1
#define SYS_pread64       17
#define SYS_pwrite64      18
#define SYS_clock_gettime 228
#define SYS_exit_group    231
#define SYS_ppoll         271

int sys_read(int fd, char *buf, size_t size)
{
    int ret;

    __asm__ __volatile__ (
            "syscall"
            : "=a" (ret)
            : "a" (SYS_read), "D" (fd), "S" (buf), "d" (size)
            : "rcx", "r11", "memory"
    );

    return ret;
}

int sys_write(int fd, const char *buf, size_t size)
{
    int ret;

    __asm__ __volatile__ (
            "syscall"
            : "=a" (ret)
            : "a" (SYS_write), "D" (fd), "S" (buf), "d" (size)
            : "rcx", "r11", "memory"
    );

    return ret;
}

int sys_pread64(int fd, char *buf, size_t size, long long pos)
{
    int ret;
    register int64_t r10 asm("r10") = pos;

    __asm__ __volatile__ (
            "syscall"
            : "=a" (ret)
            : "a" (SYS_pread64), "D" (fd), "S" (buf), "d" (size), "r" (r10)
            : "rcx", "r11", "memory"
    );

    return ret;
}

int sys_pwrite64(int fd, const char *buf, size_t size, long long pos)
{
    int ret;
    register int64_t r10 asm("r10") = pos;

    __asm__ __volatile__ (
            "syscall"
            : "=a" (ret)
            : "a" (SYS_pwrite64), "D" (fd), "S" (buf), "d" (size), "r" (r10)
            : "rcx", "r11", "memory"
    );

    return ret;
}

void sys_exit_group(int status)
{
    __asm__ __volatile__ (
            "syscall"
            :
            : "a" (SYS_exit_group), "D" (status)
            : "rcx", "r11", "memory"
    );

    for(;;);
}

int sys_clock_gettime(const int which, struct sys_timespec *ts)
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

int sys_ppoll(struct sys_pollfd *fds, unsigned int nfds,
        struct sys_timespec *ts)
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
