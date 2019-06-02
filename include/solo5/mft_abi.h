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
 * mft_abi.h: Manifest ABI definitions.
 *
 * This header file must be kept self-contained with no external dependencies
 * other than C99 headers. It defines the binary structures of the application
 * manifest.
 *
 */

#include <stdbool.h>
#include <stdint.h>

#ifndef MFT_ABI_H
#define MFT_ABI_H

/*
 * MFT_VERSION is the manifest ABI version.
 */
#define MFT_VERSION 1

enum mft_type {
    MFT_BLOCK_BASIC,
    MFT_NET_BASIC
};

struct mft_block_basic {
    uint64_t capacity;
    uint16_t block_size;
};

struct mft_net_basic {
    uint8_t mac[6];
    uint16_t mtu;
};

#define MFT_NAME_SIZE 32
#define MFT_NAME_MAX 31

struct mft_entry {
    char name[MFT_NAME_SIZE];
    enum mft_type type;
    union {
        struct mft_block_basic block_basic;
        struct mft_net_basic net_basic;
    } u;
    int hostfd;
    bool ok;
};

/*
 * MFT_ENTRIES is defined by mfttool when a manifest is being *defined*.
 * If it is not defined, then (struct mft).e will become a VLA, this is
 * intentional.
 */
#ifndef MFT_ENTRIES
#define MFT_ENTRIES
#endif

struct mft {
    uint32_t version;
    uint32_t entries;
    struct mft_entry e[MFT_ENTRIES];
};

#define SOLO5_NOTE_NAME "Solo5"
#define SOLO5_NOTE_MANIFEST 0x3154464d /* "MFT1" */

struct mft_note_header {
    uint32_t namesz;
    uint32_t descsz;
    uint32_t type;
    char name[(sizeof(SOLO5_NOTE_NAME) + 3) & -4];
};

struct mft_note {
    struct mft_note_header h;
    struct mft m;
};

/*
 * Maximum supported number of manifest entires.
 */
#define MFT_MAX_ENTRIES 64
/*
 * Maximum total size of manifest ELF note, including header.
 */
#define MFT_NOTE_MAX_SIZE (sizeof (struct mft_note) + \
        (MFT_MAX_ENTRIES * sizeof (struct mft_entry)))

#define MFT_NOTE_BEGIN \
const struct mft_note __solo5_manifest_note \
__attribute__ ((section (".note.solo5.manifest"))) \
= { \
    .h = { \
        .namesz = sizeof(SOLO5_NOTE_NAME), \
        .descsz = (sizeof(struct mft_note) - \
                   sizeof(struct mft_note_header)), \
        .type = SOLO5_NOTE_MANIFEST, \
        .name = SOLO5_NOTE_NAME \
    }, \
    .m = \

#define MFT_NOTE_END };

#endif /* MFT_ABI_H */
