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
 * hvt_seccomp_linux.c: seccomp-bpf sandbox for the hvt tender on Linux.
 *
 * Builds an allowlist of the syscalls the tender actually uses at runtime,
 * compiles it down to a BPF program via libseccomp, and installs it just
 * before the run loop starts. Anything off the list gets the process killed
 * with SIGSYS. Closes #282.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <limits.h>
#include <seccomp.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <linux/filter.h>
#include <linux/seccomp.h>

#include "hvt.h"

extern struct hvt_module __start_modules;
extern struct hvt_module __stop_modules;

static inline int _memfd_create(const char *name, unsigned int flags)
{
    return syscall(__NR_memfd_create, name, flags);
}

void hvt_seccomp_apply(void)
{
    int rc;
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL);
    assert(ctx != NULL);

    /*
     * Syscalls the run loop actually needs. Everything else gets SIGSYS.
     * No argument matching yet, per-fd checks would be a nice follow up.
     * Modules add their own syscalls below via install_seccomp_rules, so
     * debug-only modules (dumpcore, gdb) don't widen this binary's
     * filter unless they're linked in.
     */
    static const int allow[] = {
        SCMP_SYS(ioctl), /* KVM_RUN on vcpufd */
        SCMP_SYS(read), /* net tap */
        SCMP_SYS(write), /* console, net tap, stderr */
        SCMP_SYS(pread64), /* block read */
        SCMP_SYS(pwrite64), /* block write */
        SCMP_SYS(epoll_pwait), /* poll hypercall */
        SCMP_SYS(timerfd_settime), /* timeout for the poll */
        SCMP_SYS(clock_gettime), /* walltime hypercall */
        SCMP_SYS(exit_group), /* guest exit */
        SCMP_SYS(rt_sigreturn), /* signal handler returning */
        /* net I/O thread: TSYNC covers it; these are pthread/glibc internals
         * plus the arena that free(ta) spins up at thread exit. */
        SCMP_SYS(futex), /* pthread_join, mutex */
        SCMP_SYS(mmap), /* glibc arena from free(ta) */
        SCMP_SYS(mprotect), /* glibc arena setup */
        SCMP_SYS(munmap), /* glibc arena trim */
        SCMP_SYS(madvise), /* pthread stack release on exit */
        SCMP_SYS(rt_sigprocmask), /* pthread library */
        SCMP_SYS(exit), /* thread exit */
        SCMP_SYS(close), /* tap fd cleanup */
    };
    for (size_t i = 0; i < sizeof(allow) / sizeof(allow[0]); i++) {
        rc = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, allow[i], 0);
        if (rc != 0)
            errx(1, "seccomp_rule_add() failed: %s", strerror(-rc));
    }

    for (struct hvt_module *m = &__start_modules; m < &__stop_modules; m++) {
        if (m->ops.install_seccomp_rules)
            m->ops.install_seccomp_rules(ctx);
    }

    /* memfd install: clean libseccomp up before arming the filter. */
    int bpf_fd = _memfd_create("hvt_bpf_filter", 0);
    if (bpf_fd < 0)
        err(1, "memfd_create() failed");
    rc = seccomp_export_bpf(ctx, bpf_fd);
    if (rc != 0)
        errx(1, "seccomp_export_bpf() failed: %s", strerror(-rc));
    struct stat sb;
    rc = fstat(bpf_fd, &sb);
    if (rc != 0)
        err(1, "fstat() failed");
    if (lseek(bpf_fd, 0, SEEK_SET) == (off_t)-1)
        err(1, "lseek() failed");

    struct sock_filter dummy[1];
    char bpf_prgm[sb.st_size];
    ssize_t nbytes = read(bpf_fd, bpf_prgm, sb.st_size);
    assert(nbytes == sb.st_size);
    close(bpf_fd);
    seccomp_release(ctx);

    rc = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
    if (rc != 0)
        err(1, "prctl(PR_SET_NO_NEW_PRIVS) failed");
    struct sock_fprog prog = {
        .len = (unsigned short)(sb.st_size / sizeof dummy[0]),
        .filter = (struct sock_filter *)bpf_prgm};
    assert((sb.st_size / sizeof dummy[0]) <= USHRT_MAX);
    rc = syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER,
                 SECCOMP_FILTER_FLAG_TSYNC, &prog);
    if (rc != 0)
        err(1, "seccomp(SECCOMP_SET_MODE_FILTER) failed");
}
