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
 * mft_abi.h: Application manifest ABI definitions.
 *
 * This header file must be kept self-contained with no external dependencies
 * other than C99 headers. It defines the binary structures of the application
 * manifest.
 *
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef MFT_ABI_H
#define MFT_ABI_H

/*
 * MFT_VERSION is the manifest ABI version.
 */
#define MFT_VERSION 1

/*
 * Supported manifest entry types.
 */
typedef enum mft_type {
    MFT_DEV_BLOCK_BASIC = 1,
    MFT_DEV_NET_BASIC,
    MFT_RESERVED_FIRST = (1U << 30)
} mft_type_t;

/*
 * MFT_DEV_BLOCK_BASIC (basic block device) properties.
 */
struct mft_block_basic {
    uint64_t capacity;
    uint16_t block_size;
};

/*
 * MFT_DEV_NET_BASIC (basic network device) properties.
 */
struct mft_net_basic {
    uint8_t mac[6];
    uint16_t mtu;
};

#define MFT_NAME_SIZE 68        /* Bytes, including string terminator */
#define MFT_NAME_MAX  67        /* Characters */

/*
 * Manifest entry (struct mft.e[]).
 */
struct mft_entry {
    char name[MFT_NAME_SIZE];
    mft_type_t type;
    union {
        struct mft_block_basic block_basic;
        struct mft_net_basic net_basic;
    } u;
    union {
        int hostfd;             /* Backing host descriptor OR */
        void *data;             /* backing object / structure */
    } b;
    bool attached;              /* Device attached? */
};

/*
 * Maximum supported number of manifest entires.
 */
#define MFT_MAX_ENTRIES 64

/*
 * Ensure that MFT_MAX_ENTRIES is not larger than the number of bits in a
 * solo5_handle_set_t (actually a uint64_t in disguise, but we don't want to
 * depend on solo5.h here).
 */
_Static_assert(MFT_MAX_ENTRIES <= (sizeof(uint64_t) * 8),
        "MFT_MAX_ENTRIES too large for solo5_handle_set_t");

/*
 * MFT_ENTRIES is defined by elftool when a manifest is being *defined*.
 * If it is not defined, then (struct mft).e will become a VLA, this is
 * intentional.
 */
#ifndef MFT_ENTRIES
#define MFT_ENTRIES
#else
_Static_assert(MFT_ENTRIES <= MFT_MAX_ENTRIES, "MFT_ENTRIES out of range");
#endif

/*
 * Top-level manifest structure.
 */
struct mft {
    uint32_t version;
    uint32_t entries;
    struct mft_entry e[MFT_ENTRIES];
};

/*
 * HERE BE DRAGONS.
 *
 * The following structures and macros are used to declare a Solo5 "MFT1"
 * format note at link time. This is somewhat tricky, as we need to ensure all
 * structures are aligned correctly.
 */
#define MFT1_NOTE_NAME "Solo5"
#define MFT1_NOTE_TYPE 0x3154464d /* "MFT1" */

/*
 * Defines an Elf64_Nhdr with n_name filled in and padded to a 4-byte boundary,
 * i.e. the common part of a Solo5-owned Nhdr.
 */
struct mft1_nhdr {
    uint32_t n_namesz;
    uint32_t n_descsz;
    uint32_t n_type;
    char n_name[(sizeof(MFT1_NOTE_NAME) + 3) & -4];
};

_Static_assert((sizeof(struct mft1_nhdr)) ==
        (sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) + 8),
        "struct mft1_nhdr alignment issue");

/*
 * Defines the entire note (header, descriptor content).
 */
struct mft1_note {
    struct mft1_nhdr h;
    struct mft m;
};

/*
 * Internal alignment of (m) within struct mft1_note. Must be passed to
 * elf_load_note() as note_align when loading.
 */
#define MFT1_NOTE_ALIGN offsetof(struct { char c; struct mft m; }, m)

_Static_assert((offsetof(struct mft1_note, m) & (MFT1_NOTE_ALIGN - 1)) == 0,
        "struct mft1_note.m is not aligned to a MFT1_NOTE_ALIGN boundary");

/*
 * Maximum descsz of manifest ELF note descriptor (content), including
 * internal alignment.
 */
#define MFT1_NOTE_MAX_SIZE ((sizeof (struct mft1_note) - \
         sizeof (struct mft1_nhdr)) + \
        (MFT_MAX_ENTRIES * sizeof (struct mft_entry)))

/*
 * Declare a Solo5 "MFT1" format NOTE.
 *
 * Structure must be aligned to a 4-byte boundary (or possibly an 8-byte
 * boundary, but ELF64 toolchains seem happy with the current arrangement...
 * the specifications are mess).
 */
#define MFT1_NOTE_DECLARE_BEGIN \
const struct mft1_note __solo5_mft1_note \
__attribute__ ((section (".note.solo5.manifest"), aligned(4))) \
= { \
    .h = { \
        .n_namesz = sizeof(MFT1_NOTE_NAME), \
        .n_descsz = (sizeof(struct mft1_note) - \
                    sizeof(struct mft1_nhdr)), \
        .n_type = MFT1_NOTE_TYPE, \
        .n_name = MFT1_NOTE_NAME \
    }, \
    .m =

#define MFT1_NOTE_DECLARE_END };

#endif /* MFT_ABI_H */
