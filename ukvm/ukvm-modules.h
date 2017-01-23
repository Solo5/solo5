/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of ukvm, a unikernel monitor.
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

#ifndef __UKVM_MODULES_H__
#define __UKVM_MODULES_H__

/* hypercall interfaces exported by modules are in ukvm.h */

struct ukvm_module {
    int (*get_fd)(void);
    int (*handle_exit)(struct kvm_run *run, int vcpufd, uint8_t *mem);
    int (*handle_cmdarg)(char *cmdarg);
    int (*setup)(int vcpufd, uint8_t *mem);
    char *(*usage)(void);
    const char *name;
};

extern struct ukvm_module ukvm_blk;
extern struct ukvm_module ukvm_net;
extern struct ukvm_module ukvm_gdb;

#endif
