/*
 * Copyright (c) 2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a unikernel base layer.
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

#ifndef MUSINFO_H
#define MUSINFO_H

#include "sinfo.h"

#define MUEN_SUBJECT_INFO_MAGIC 0x01006f666e69756dULL
#define MAX_RESOURCE_COUNT      255
#define NO_RESOURCE             0
#define NO_PATTERN              256

struct name_type {
    uint8_t length;
    char data[MAX_NAME_LENGTH];
} __attribute__((packed));

#define MEM_WRITABLE_FLAG   (1 << 0)
#define MEM_EXECUTABLE_FLAG (1 << 1)

struct memregion_type {
    enum muen_content_type content;
    uint64_t address;
    uint64_t size;
    uint8_t hash[HASH_LENGTH];
    uint8_t flags;
    uint16_t pattern;
    char padding[1];
} __attribute__((packed));

#define CHAN_EVENT_FLAG  (1 << 0)
#define CHAN_VECTOR_FLAG (1 << 1)

struct channel_info_type {
    uint8_t flags;
    uint8_t event;
    uint8_t vector;
    char padding[5];
} __attribute__((packed));

struct resource_type {
    struct name_type name;
    uint8_t memregion_idx;
    uint8_t channel_info_idx;
    char padding[6];
} __attribute__((packed));

struct dev_info_type {
    uint16_t sid;
    uint16_t irte_start;
    uint8_t irq_start;
    uint8_t ir_count;
    uint8_t flags;
    char padding[1];
} __attribute__((packed));

#define DEV_MSI_FLAG  (1 << 0)

struct subject_info_type {
    uint64_t magic;
    struct name_type name;
    uint8_t resource_count;
    uint8_t memregion_count;
    uint8_t channel_info_count;
    uint8_t dev_info_count;
    char padding[4];
    uint64_t tsc_khz;
    struct resource_type resources[MAX_RESOURCE_COUNT];
    struct memregion_type memregions[MAX_RESOURCE_COUNT];
    struct channel_info_type channels_info[MAX_RESOURCE_COUNT];
    struct dev_info_type dev_info[MAX_RESOURCE_COUNT];
} __attribute__((packed));

#endif
