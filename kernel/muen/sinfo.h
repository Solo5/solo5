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

#ifndef __MUEN_SINFO_H__
#define __MUEN_SINFO_H__

#define MAX_NAME_LENGTH 63
#define HASH_LENGTH 32

/*
 * Muen subject information API.
 *
 * Defines functions to retrieve information about the execution environment
 * of a Linux subject running on the Muen Separation Kernel.
 */

/* Structure holding information about a memory region */
enum muen_content_type {
    MUEN_CONTENT_UNINITIALIZED, MUEN_CONTENT_FILL, MUEN_CONTENT_FILE
};

struct muen_memregion_info {
    char name[MAX_NAME_LENGTH + 1];
    enum muen_content_type content;
    uint64_t address;
    uint64_t size;
    bool writable;
    bool executable;
    uint8_t hash[HASH_LENGTH];
    uint16_t pattern;
};

/* Structure holding information about a Muen channel */
struct muen_channel_info {
    char name[MAX_NAME_LENGTH + 1];
    uint64_t address;
    uint64_t size;
    uint8_t event_number;
    uint8_t vector;
    bool writable;
    bool has_event;
    bool has_vector;
};

/* Structure holding information about PCI devices */
struct muen_dev_info {
    uint16_t sid;
    uint16_t irte_start;
    uint8_t irq_start;
    uint8_t ir_count;
    bool msi_capable;
};

/*
 * Check Muen sinfo Magic.
 */
bool muen_check_magic(void);

/*
 * Return subject name.
 *
 * The function returns NULL if the subject name cannot be retrieved.
 */
const char * muen_get_subject_name(void);

/*
 * Return information for a channel given by name.
 *
 * If no channel with given name exists, False is returned. The event_number
 * and vector parameters are only valid if indicated by the has_[event|vector]
 * struct members.
 */
bool muen_get_channel_info(const char * const name,
               struct muen_channel_info *channel);

/*
 * Return information for a memory region given by name.
 *
 * If no memory region with given name exists, False is returned.
 */
bool muen_get_memregion_info(const char * const name,
                 struct muen_memregion_info *memregion);

/*
 * Return information for PCI device with given SID.
 *
 * The function returns false if no device information for the specified device
 * exists.
 */
bool muen_get_dev_info(const uint16_t sid, struct muen_dev_info *dev);

/*
 * Channel callback.
 *
 * Used in the muen_for_each_channel function. The optional void data pointer
 * can be used to pass additional data.
 */
typedef bool (*channel_cb)(const struct muen_channel_info * const channel,
        void *data);

/*
 * Invoke given callback function for each available channel.
 *
 * Channel information and the optional data argument are passed to each
 * invocation of the callback. If a callback invocation returns false,
 * processing is aborted and false is returned to the caller.
 */
bool muen_for_each_channel(channel_cb func, void *data);

/*
 * Memory region callback.
 *
 * Used in the muen_for_each_memregion function. The optional void data pointer
 * can be used to pass additional data.
 */
typedef bool (*memregion_cb)(const struct muen_memregion_info * const memregion,
        void *data);

/*
 * Invoke given callback function for each available memory region.
 *
 * Memory region information and the optional data argument are passed to each
 * invocation of the callback. If a callback invocation returns false,
 * processing is aborted and false is returned to the caller.
 */
bool muen_for_each_memregion(memregion_cb func, void *data);

/*
 * Return TSC tick rate in kHz.
 *
 * The function returns 0 if the TSC tick rate cannot be retrieved.
 */
uint64_t muen_get_tsc_khz(void);

/*
 * Return start time of current minor frame in TSC ticks.
 *
 * The function returns 0 if the start time cannot be retrieved.
 */
uint64_t muen_get_sched_start(void);

/*
 * Return end time of current minor frame in TSC ticks.
 *
 * The function returns 0 if the end time cannot be retrieved.
 */
uint64_t muen_get_sched_end(void);

#endif
