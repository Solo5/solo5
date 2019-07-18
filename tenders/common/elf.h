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
 * elf.h: ELF loader APIs.
 */

#ifndef COMMON_ELF_H
#define COMMON_ELF_H

#include "mft_abi.h"

typedef int (*guest_mprotect_fn_t)(void *t, void *addr, size_t size, int prot);

/*
 * Load an ELF binary from (file) into (mem_size) bytes of (mem), returning
 * the entry point (p_entry), last byte used by the binary (p_end).
 */
void elf_load(void *t, const char *file, uint8_t *mem, size_t mem_size,
        guest_mprotect_fn_t guest_mprotect_fn,
        uint64_t *p_entry, uint64_t *p_end);

/*
 * Load the Solo5 manifest from the ELF binary (file). Memory for the manifest
 * is allocated with malloc() and returned as (mft), with the manifest size as
 * defined in the ELF binary returned in (mft_size).
 */
void elf_load_mft(const char *file, struct mft **mft, size_t *mft_size);

#endif /* COMMON_ELF_H */
