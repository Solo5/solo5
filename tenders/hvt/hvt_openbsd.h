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

/*
 * hvt_openbsd.h: OpenBSD vmm(4) backend definitions.
 */

#ifndef HVT_HV_OPENBSD_H
#define HVT_HV_OPENBSD_H

#define VMM_NODE    "/dev/vmm"
#define VMD_USER    "_vmd"

struct hvt_b {
    int      vmd_fd;
    uint32_t vcp_id;
    uint32_t vcpu_id;
    struct   vcpu_reg_state *vrs;
};

/*
the following code has been extracted from musl
arch/x86_64/bits/user.h
include/sys/procfs.h

musl as a whole is licensed under the following standard MIT license:

----------------------------------------------------------------------
Copyright © 2005-2014 Rich Felker, et al.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
----------------------------------------------------------------------
*/
struct user_regs_struct {
        unsigned long r15, r14, r13, r12, rbp, rbx, r11, r10, r9, r8;
        unsigned long rax, rcx, rdx, rsi, rdi, orig_rax, rip;
        unsigned long cs, eflags, rsp, ss, fs_base, gs_base, ds, es, fs, gs;
};

#define ELF_NGREG 27
typedef unsigned long long elf_gregset_t[ELF_NGREG];

struct elf_siginfo {
	int si_signo;
	int si_code;
	int si_errno;
};

struct elf_prstatus {
	struct elf_siginfo pr_info;
	short int pr_cursig;
	unsigned long int pr_sigpend;
	unsigned long int pr_sighold;
	pid_t pr_pid;
	pid_t pr_ppid;
	pid_t pr_pgrp;
	pid_t pr_sid;
	struct timeval pr_utime;
	struct timeval pr_stime;
	struct timeval pr_cutime;
	struct timeval pr_cstime;
	elf_gregset_t pr_reg;
	int pr_fpvalid;
};
typedef struct elf_prstatus prstatus_t;
#endif /* HVT_HV_OPENBSD_H */
