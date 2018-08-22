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

#ifndef MUEN_CHANNEL_READER_H
#define MUEN_CHANNEL_READER_H

#include "channel.h"

enum muchannel_reader_result {
    MUCHANNEL_INACTIVE,
    MUCHANNEL_INCOMPATIBLE_INTERFACE,
    MUCHANNEL_EPOCH_CHANGED,
    MUCHANNEL_NO_DATA,
    MUCHANNEL_OVERRUN_DETECTED,
    MUCHANNEL_SUCCESS
};

struct muchannel_reader {
    uint64_t epoch;
    uint64_t protocol;
    uint64_t size;
    uint64_t elements;
    uint64_t rc;
};

/*
 * Initialize reader with given protocol.
 */
void muen_channel_init_reader(struct muchannel_reader *reader, uint64_t protocol);

/*
 * Read next element from given channel.
 */
enum muchannel_reader_result muen_channel_read(
        const struct muchannel * const channel,
        struct muchannel_reader *reader,
        void *element);

/*
 * Drain all current channel elements.
 */
void muen_channel_drain(const struct muchannel * const channel,
            struct muchannel_reader *reader);

/*
 * Returns True if there is pending data in the channel.
 */
bool muen_channel_has_pending_data(const struct muchannel * const channel,
        struct muchannel_reader *reader);

#endif
