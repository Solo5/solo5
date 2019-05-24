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
 * The sys_ functions in this file are intentionally weakly typed as they only
 * pass through values to/from the system call without interpretation. All
 * integer values are passed as (long) and all pointer values are passed as
 * (void *).
 */

#define SYS_read          3
#define SYS_write         4
#define SYS_pread64       179
#define SYS_pwrite64      180
#define SYS_clock_gettime 246
#define SYS_exit_group    234
#define SYS_ppoll         281

long sys_read(long fd, void *buf, long size)
{
    register long r0 __asm__("r0") = SYS_read;
    register long r3 __asm__("r3") = fd;
    register long r4 __asm__("r4") = (long)buf;
    register long r5 __asm__("r5") = size;

    __asm__ __volatile__ (
            "sc"
            : "=r" (r3)
            : "r" (r0), "r" (r3), "r" (r4), "r" (r5)
            : "memory", "cc"
    );

    return r3;
}

long sys_write(long fd, const void *buf, long size)
{
    register long r0 __asm__("r0") = SYS_write;
    register long r3 __asm__("r3") = fd;
    register long r4 __asm__("r4") = (long)buf;
    register long r5 __asm__("r5") = size;

    __asm__ __volatile__ (
            "sc"
            : "=r" (r3)
            : "r" (r0), "r" (r3), "r" (r4), "r" (r5)
            : "memory", "cc"
    );

    return r3;
}

long sys_pread64(long fd, void *buf, long size, long pos)
{
    register long r0 __asm__("r0") = SYS_pread64;
    register long r3 __asm__("r3") = fd;
    register long r4 __asm__("r4") = (long)buf;
    register long r5 __asm__("r5") = size;
    register long r6 __asm__("r6") = pos;

    __asm__ __volatile__ (
            "sc"
            : "=r" (r3)
            : "r" (r0), "r" (r3), "r" (r4), "r" (r5), "r" (r6)
            : "memory", "cc"
    );

    return r3;
}

long sys_pwrite64(long fd, const void *buf, long size, long pos)
{
    register long r0 __asm__("r0") = SYS_pwrite64;
    register long r3 __asm__("r3") = fd;
    register long r4 __asm__("r4") = (long)buf;
    register long r5 __asm__("r5") = size;
    register long r6 __asm__("r6") = pos;

    __asm__ __volatile__ (
            "sc"
            : "=r" (r3)
            : "r" (r0), "r" (r3), "r" (r4), "r" (r5), "r" (r6)
            : "memory", "cc"
    );

    return r3;
}

void sys_exit_group(long status)
{
    register long r0 __asm__("r0") = SYS_exit_group;
    register long r3 __asm__("r3") = status;
    __asm__ __volatile__ (
            "sc"
            : "=r" (r0)
            : "r" (r0), "r" (r3)
            : "memory", "cc"
    );

    for(;;);
}

long sys_clock_gettime(const long which, void *ts)
{
    register long r0 __asm__("r0") = SYS_clock_gettime;
    register long r3 __asm__("r3") = which;
    register long r4 __asm__("r4") = (long)ts;

    __asm__ __volatile__ (
            "sc"
            : "=r" (r3)
            : "r" (r0), "r" (r3), "r" (r4)
            : "memory", "cc"
    );

    return r3;
}

long sys_ppoll(void *fds, long nfds, void *ts)
{
    register long r0 __asm__("r0") = SYS_ppoll;
    register long r3 __asm__("r3") = (long)fds;
    register long r4 __asm__("r4") = nfds;
    register long r5 __asm__("r5") = (long)ts;
    register long r6 __asm__("r6") = 0; /* sigmask */
    register long r7 __asm__("r7") = 0; /* sigsetsize */

    __asm__ __volatile__ (
            "sc"
            : "=r" (r3)
            : "r" (r0), "r" (r3), "r" (r4), "r" (r5), "r" (r6), "r" (r7)
            : "memory", "cc"
    );

    return r3;
}
