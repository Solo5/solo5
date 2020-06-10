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
 * spt.h: spt tender internal API definitions.
 *
 */

#ifndef SPT_H
#define SPT_H

#if defined(__BITS_32__)
#define ALIGN_SIZE 4
#elif defined(__BITS_64__)
#define ALIGN_SIZE 8
#else
#error Unsupported architecture
#endif

#include <inttypes.h>
#include <err.h>

#include "../common/cc.h"
#include "../common/elf.h"
#include "../common/mft.h"
#include "../common/types.h"
#include "spt_abi.h"

struct spt {
    uint8_t *mem;
    size_t mem_size;
    struct spt_boot_info *bi;
    int epollfd;
    int timerfd;
    void *sc_ctx;
};

struct spt *spt_init(size_t mem_size);

int spt_guest_mprotect(void *t_arg, addr_t addr_start, addr_t addr_end,
        int prot);

void spt_boot_info_init(struct spt *spt, addr_t p_end, int cmdline_argc,
	char **cmdline_argv, struct mft *mft, size_t mft_size);

void spt_run(struct spt *spt, addr_t p_entry);

/*
 * Operations provided by a module. (setup) is required, all other functions
 * are optional.
 */
struct spt_module_ops {
    int (*setup)(struct spt *spt, struct mft *mft);
    int (*handle_cmdarg)(char *cmdarg, struct mft *mft);
    char *(*usage)(void);
};

struct spt_module {
    const char name[32];
    struct spt_module_ops ops;
};

/*
 * Declare the module (module_name).
 *
 * Usage:
 *
 * DECLARE_MODULE(module_name, <initializer of struct spt_module_ops>);
 *
 * Note that alignment of the struct is explicitly set, otherwise the linker
 * will pick a default that does not match the compiler's alignment.
 */
#define DECLARE_MODULE(module_name, ...) \
    static struct spt_module __module_ ##module_name \
    __attribute((section("modules"), aligned(ALIGN_SIZE))) \
    __attribute((used)) = { \
	.name = #module_name, \
	.ops = { __VA_ARGS__ } \
    };

#endif /* SPT_H */
