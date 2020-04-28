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

#include "bindings.h"

const struct mft *muen_manifest = NULL;

extern const struct mft1_note __solo5_mft1_note;

void platform_init(const void *arg)
{
    process_bootinfo(arg);

    /*
     * Get the built-in manifest out of the ELF NOTE and validate it.
     * Once validated, it is available for access globally by the bindings.
     */
    const struct mft *mft;
    size_t mft_size;
    mft_get_builtin_mft1(&__solo5_mft1_note, &mft, &mft_size);
    if (mft_validate(mft, mft_size) != 0) {
        log(ERROR, "Solo5: Built-in manifest validation failed. Aborting.\n");
        solo5_abort();
    }
    muen_manifest = mft;
}

void platform_exit(int status __attribute__((unused)),
    void *cookie __attribute__((unused)))
{
    const char msg[] = "Solo5: Halted\n";
    platform_puts(msg, strlen(msg));
    __asm__ __volatile__("cli; hlt");
    for (;;);
}
