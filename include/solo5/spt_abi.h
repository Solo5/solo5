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
 * spt_abi.h: spt guest/tender interface definitions.
 *
 * This header file must be kept self-contained with no external dependencies
 * other than C99 headers.
 */

#ifndef SPT_ABI_H
#define SPT_ABI_H

#include <stddef.h>
#include <stdint.h>

/*
 * ABI version. This must be incremented before cutting a release of Solo5 if
 * any material changes are made to the interfaces or data structures defined
 * in this file.
 */

#define SPT_ABI_VERSION 1

/*
 * ABI target.
 */

/* #define HVT_ABI_TARGET 1 */
#define SPT_ABI_TARGET 2
/* #define VIRTIO_ABI_TARGET 3 */
/* #define MUEN_ABI_TARGET 4 */
/* #define GENODE_ABI_TARGET 5 */

/*
 * Lowest virtual address at which guests can be loaded.
 */
#define SPT_GUEST_MIN_BASE 0x100000

/*
 * A pointer to this structure is passed by the tender as the sole argument to
 * the guest entrypoint.
 */
struct spt_boot_info {
    uint64_t mem_size;                  /* Memory size in bytes */
    uint64_t kernel_end;                /* Address of end of kernel */
    const char * cmdline;               /* Address of command line (C string) */
    void *mft;                          /* Address of application manifest */
    int epollfd;                        /* epoll() set for yield() */
    int timerfd;                        /* internal timerfd for yield() */
};

/*
 * Identifier (data.u64) for internal timerfd in epoll() set.
 */
#define SPT_INTERNAL_TIMERFD (~1U)

/*
 * The lowest memory address at which we can mmap() memory on the host. See
 * spt_main.c for an explanation.
 */
#define SPT_HOST_MEM_BASE 0x10000

/*
 * Guest low memory layout.
 */
#define SPT_BOOT_INFO_BASE (SPT_HOST_MEM_BASE + 0x1000)

/*
 * Maximum size of guest command line, including the string terminator.
 */
#define SPT_CMDLINE_SIZE 8192

/*
 * HERE BE DRAGONS.
 *
 * The following structures and macros are used to declare a Solo5 "ABI1"
 * format note at link time. This is somewhat tricky, as we need to ensure all
 * structures are aligned correctly.
 */
#define ABI1_NOTE_NAME "Solo5"
#define ABI1_NOTE_TYPE 0x31494241 /* "ABI1" */

/*
 * Defines an Elf64_Nhdr with n_name filled in and padded to a 4-byte boundary,
 * i.e. the common part of a Solo5-owned Nhdr.
 */
struct abi1_nhdr {
    uint32_t n_namesz;
    uint32_t n_descsz;
    uint32_t n_type;
    char n_name[(sizeof(ABI1_NOTE_NAME) + 3) & -4];
};

_Static_assert((sizeof(struct abi1_nhdr)) ==
        (sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) + 8),
        "struct abi1_nhdr alignment issue");

/*
 * Defines the entire note (header, descriptor content).
 */
struct abi1_info {
    uint32_t abi_target;
    uint32_t abi_version;
    uint32_t reserved0;
    uint32_t reserved1;
};

struct abi1_note {
    struct abi1_nhdr h;
    struct abi1_info i;
};

/*
 * Internal alignment of (i) within struct abi1_note. Must be passed to
 * elf_load_note() as note_align when loading.
 */
#define ABI1_NOTE_ALIGN offsetof(struct { char c; struct abi1_info i; }, i)

_Static_assert((offsetof(struct abi1_note, i) & (ABI1_NOTE_ALIGN - 1)) == 0,
        "struct abi1_note.i is not aligned to a ABI1_NOTE_ALIGN boundary");

/*
 * Maximum descsz of ABI1 ELF note descriptor (content), including
 * internal alignment.
 */
#define ABI1_NOTE_MAX_SIZE ((sizeof (struct abi1_note) - \
         sizeof (struct abi1_nhdr) + sizeof (struct abi1_info)))

/*
 * Declare a Solo5 "ABI1" format NOTE.
 *
 * Structure must be aligned to a 4-byte boundary (or possibly an 8-byte
 * boundary, but ELF64 toolchains seem happy with the current arrangement...
 * the specifications are mess).
 */
#define ABI1_NOTE_DECLARE_BEGIN \
const struct abi1_note __solo5_abi1_note \
__attribute__ ((section (".note.solo5.abi"), aligned(4))) \
= { \
    .h = { \
        .n_namesz = sizeof(ABI1_NOTE_NAME), \
        .n_descsz = (sizeof(struct abi1_note) - \
                    sizeof(struct abi1_nhdr)), \
        .n_type = ABI1_NOTE_TYPE, \
        .n_name = ABI1_NOTE_NAME \
    }, \
    .i =

#define ABI1_NOTE_DECLARE_END };

#endif /* SPT_ABI_H */
