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

#include <inttypes.h>
#include <err.h>

#include "../common/cc.h"
#include "../common/elf.h"
#include "spt_abi.h"

struct spt {
    uint8_t *mem;
    size_t mem_size;
    struct spt_boot_info *bi;
    void *sc_ctx;
    uint64_t tls_base;
};

struct spt *spt_init(size_t mem_size);

void spt_bi_init(struct spt *spt, uint64_t p_end, char **cmdline,
                 uint32_t tls_size);

void spt_run(struct spt *spt, uint64_t p_entry);

/*
 * Module definition. (name) and (setup) are required, all other functions are
 * optional.
 */
struct spt_module {
    const char *name;
    int (*setup)(struct spt *spt);
    int (*handle_cmdarg)(char *cmdarg);
    char *(*usage)(void);
};

extern struct spt_module spt_module_net;
extern struct spt_module spt_module_block;

/*
 * Array of compiled-in modules. NULL terminated.
 */
extern struct spt_module *spt_core_modules[];

#endif /* SPT_H */
