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
 * mft.h: Application manifest validation and manipulation.
 */

#ifndef MFT_H
#define MFT_H

#include <stddef.h>
#include "mft_abi.h"

/*
 * Validate that the manfiest at (mft), of size (mft_size) is safe to use, and
 * sanitize all fields with prejudice, as these are considered untrusted data.
 *
 * On success returns 0, on error returns -1.
 */
int mft_validate(struct mft *mft, size_t mft_size);

/*
 * Return the manifest entry matching (name), of type (type), or NULL if none
 * found.
 */
struct mft_entry *mft_get_by_name(struct mft *mft, const char *name,
        mft_type_t type, unsigned *index);

/*
 * Return the manifest entry at (index), of type (type), or NULL if the entry
 * at (index) is not of type (type).
 */

struct mft_entry *mft_get_by_index(struct mft *mft, unsigned index,
        mft_type_t type);

/*
 * Return a string representation of (type).
 */
const char *mft_type_to_string(mft_type_t type);

#endif /* MFT_H */
