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
 * bindings.h: Solo5 bindings, muen implementation additions.
 *
 * This header file includes (supersedes) the common bindings.h for the muen
 * implementation.
 *
 * TODO: Remove the dependency on hvt_abi.h entirely?
 */

#ifndef __MUEN_BINDINGS_H__
#define __MUEN_BINDINGS_H__

#include "../bindings.h"
#include "hvt_abi.h"
#include "elf_abi.h"

void time_init(const struct hvt_boot_info *bi);
void console_init(void);
void net_init(const struct hvt_boot_info *bi);
void block_init(const struct hvt_boot_info *bi);

/* muen-clock.c: TSC-based clock */
uint64_t tscclock_monotonic(void);
int tscclock_init(uint64_t tsc_freq);
uint64_t tscclock_epochoffset(void);

void process_bootinfo(const void *arg);

#endif /* __MUEN_BINDINGS_H__ */
