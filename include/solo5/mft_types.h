#include <stdint.h>

#ifndef MFT_TYPES_H
#define MFT_TYPES_H

enum mft_type {
    MFT_BLOCK_BASIC,
    MFT_NET_BASIC
};

struct mft_block_basic {
    uint64_t capacity;
    uint16_t block_size;
    void *private;
};

struct mft_net_basic {
    uint8_t mac[6];
    uint16_t mtu;
    void *private;
};

struct mft_entry {
    char name[32];
    enum mft_type type;
    union {
        struct mft_block_basic block_basic;
        struct mft_net_basic net_basic;
    };
};

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

#endif /* MFT_TYPES_H */
