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
 * mft.c: Application manifest validation and manipulation.
 */

#ifndef __SOLO5_BINDINGS__
#include <assert.h>
#include <string.h>
#endif

#include "mft.h"

int mft_validate(const struct mft *mft, size_t mft_size)
{
    /*
     * Note that this function is called early in the loading process by the
     * tenders, and must treat the manifest at (*mft) and supplied (mft_size)
     * as untrusted data.
     */
    if (mft_size < sizeof(*mft))
        return -1;
    if (mft->version != MFT_VERSION)
        return -1;
    if (mft->entries > MFT_MAX_ENTRIES)
        return -1;
    /*
     * mft_size must be the exact expected structure size, including the space
     * required for manifest entires.
     *
     * If you are debugging this and it does not match up, the most likely
     * cause is an internal structure alignment issue (see mft_abi.h).
     */
    if (mft_size != (sizeof(struct mft) +
                (mft->entries * sizeof(struct mft_entry))))
        return -1;
    /*
     * The manifest must contain at least one entry, and the first entry must
     * be of type MFT_RESERVED_FIRST with an empty name.
     */
    if (mft->entries < 1)
        return -1;
    if (mft->e[0].type != MFT_RESERVED_FIRST)
        return -1;
    if (mft->e[0].name[0] != 0)
        return -1;

    for (unsigned i = 0; i != mft->entries; i++) {
        /*
         * Ensure .name[] is always terminated with \0, even if there is garbage
         * in the array itself.
         */
        if (mft->e[i].name[MFT_NAME_MAX] != 0)
            return -1;
        /*
         * Sanitize private fields (to be used by the tender/bindings):
         *
         * 1. .attached must be false (which implies that both .b and .u are
         * uninitialised).
         */
        if (mft->e[i].attached != false)
           return -1;
    }

    return 0;
}

void mft_get_builtin_mft1_unconst(const struct mft1_note *note,
        struct mft **out_mft, size_t *out_mft_size)
{
    /*
     * Get the built-in manifest out of the ELF NOTE. Note that the size must
     * be adjusted from n_descsz to remove any internal alignment.
     */
    *out_mft = (struct mft *)&note->m;
    *out_mft_size = note->h.n_descsz -
        (offsetof(struct mft1_note, m) - sizeof (struct mft1_nhdr));
}

void mft_get_builtin_mft1(const struct mft1_note *note,
        const struct mft **out_mft, size_t *out_mft_size)
{
    /*
     * Get the built-in manifest out of the ELF NOTE. Note that the size must
     * be adjusted from n_descsz to remove any internal alignment.
     */
    *out_mft = &note->m;
    *out_mft_size = note->h.n_descsz -
        (offsetof(struct mft1_note, m) - sizeof (struct mft1_nhdr));
}

struct mft_entry *mft_get_by_name(const struct mft *mft, const char *name,
        mft_type_t type, unsigned *index)
{
    for (unsigned i = 0; i != mft->entries; i++) {
        if (mft->e[i].type == type
                && strncmp(mft->e[i].name, name, MFT_NAME_SIZE) == 0) {
            if (index != NULL)
                *index = i;
            return (struct mft_entry *)&mft->e[i];
        }
    }
    return NULL;
}

struct mft_entry *mft_get_by_index(const struct mft *mft, unsigned index,
        mft_type_t type)
{
    if (index >= mft->entries)
        return NULL;
    else if (mft->e[index].type == type)
        return (struct mft_entry *)&mft->e[index];
    else
        return NULL;
}

const char *mft_type_to_string(mft_type_t type)
{
    switch(type) {
        case MFT_DEV_BLOCK_BASIC:
            return "BLOCK_BASIC";
        case MFT_DEV_NET_BASIC:
            return "NET_BASIC";
        case MFT_DEV_PCI_BASIC:
            return "PCI_BASIC";
        case MFT_RESERVED_FIRST:
            return "RESERVED_FIRST";
        default:
            assert(false);
    }
}
