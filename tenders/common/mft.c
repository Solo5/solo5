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

int mft_validate(struct mft *mft, size_t mft_size)
{
    /*
     * Note that this function is called early in the loading process by the
     * tenders, and must treat the manifest at (*mft) and supplied (mft_size)
     * as untrusted data.
     */
    if (mft->version != MFT_VERSION)
        return -1;
    if (mft->entries > MFT_MAX_ENTRIES)
        return -1;
    if (mft_size != (sizeof(struct mft) +
                (mft->entries * sizeof(struct mft_entry))))
        return -1;

    for (unsigned i = 0; i != mft->entries; i++) {
        /* Ensure name[] is always terminated */
        mft->e[i].name[sizeof mft->e[i].name - 1] = 0;
        /* hostfd must be set to an invalid value, attached to false */
        mft->e[i].hostfd = -1;
        mft->e[i].attached = false;
        /* and device properties are zeroed */
        memset(&mft->e[i].u, 0, sizeof mft->e[i].u);
    }

    return 0;
}

struct mft_entry *mft_get_by_name(struct mft *mft, const char *name,
        mft_type_t type, unsigned *index)
{
    for (unsigned i = 0; i != mft->entries; i++) {
        if (strcmp(mft->e[i].name, name) == 0 && mft->e[i].type == type) {
            if (index != NULL)
                *index = i;
            return &mft->e[i];
        }
    }
    return NULL;
}

struct mft_entry *mft_get_by_index(struct mft *mft, unsigned index,
        mft_type_t type)
{
    if (index >= mft->entries)
        return NULL;
    else if (mft->e[index].type == type)
        return &mft->e[index];
    else
        return NULL;
}

const char *mft_type_to_string(mft_type_t type)
{
    switch(type) {
        case MFT_BLOCK_BASIC:
            return "BLOCK_BASIC";
        case MFT_NET_BASIC:
            return "NET_BASIC";
        default:
            assert(false);
    }
}
