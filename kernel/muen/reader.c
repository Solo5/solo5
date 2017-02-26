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

#include "reader.h"
#include "util.h"

static bool has_epoch_changed(const struct muchannel * const channel,
                  const struct muchannel_reader * const reader)
{
    uint64_t epoch;
    serialized_copy(&channel->hdr.epoch, &epoch);
    return reader->epoch != epoch;
}

static enum muchannel_reader_result synchronize(
        const struct muchannel * const channel,
        struct muchannel_reader *reader)
{
    enum muchannel_reader_result result;
    uint64_t proto, transport;

    serialized_copy(&channel->hdr.protocol, &proto);
    serialized_copy(&channel->hdr.transport, &transport);

    if (reader->protocol == proto && SHMSTREAM20 == transport)
    {
        serialized_copy(&channel->hdr.epoch, &reader->epoch);
        serialized_copy(&channel->hdr.size, &reader->size);
        serialized_copy(&channel->hdr.elements, &reader->elements);
        reader->rc = 0;

        result = MUCHANNEL_EPOCH_CHANGED;
    } else
        result = MUCHANNEL_INCOMPATIBLE_INTERFACE;

    return result;
}

void muen_channel_init_reader(struct muchannel_reader *reader, uint64_t protocol)
{
    reader->epoch = MUCHANNEL_NULL_EPOCH;
    reader->protocol = protocol;
    reader->size = 0;
    reader->elements = 0;
    reader->rc = 0;
}

enum muchannel_reader_result muen_channel_read(
        const struct muchannel * const channel,
        struct muchannel_reader *reader,
        void *element)
{
    uint64_t epoch, pos, wc, wsc;
    enum muchannel_reader_result result;

    if (muen_channel_is_active(channel)) {

        if (reader->epoch == MUCHANNEL_NULL_EPOCH ||
                has_epoch_changed(channel, reader))
            return synchronize(channel, reader);

        serialized_copy(&channel->hdr.wc, &wc);
        if (reader->rc == wc)
            result = MUCHANNEL_NO_DATA;
        else if (wc - reader->rc > reader->elements)
        {
            result = MUCHANNEL_OVERRUN_DETECTED;
        }
        else
        {
            pos = reader->rc % reader->elements * reader->size;
            memcpy(element, channel->data + pos, reader->size);
            cc_barrier();

            serialized_copy(&channel->hdr.wsc, &wsc);
            if (wsc - reader->rc > reader->elements) {
                result = MUCHANNEL_OVERRUN_DETECTED;
            } else {
                result = MUCHANNEL_SUCCESS;
                reader->rc++;
            }
            if (has_epoch_changed(channel, reader)) {
                result = MUCHANNEL_EPOCH_CHANGED;
                epoch = 0;
                serialized_copy(&epoch, &reader->epoch);
            }
        }
    } else {
        reader->epoch = MUCHANNEL_NULL_EPOCH;
        result = MUCHANNEL_INACTIVE;
    }

    return result;
}

void muen_channel_drain(const struct muchannel * const channel,
            struct muchannel_reader *reader)
{
    serialized_copy(&channel->hdr.wc, &reader->rc);
}

bool muen_channel_has_pending_data(const struct muchannel * const channel,
        struct muchannel_reader * reader)
{
    enum muchannel_reader_result res;
    uint64_t wc;

    if (!muen_channel_is_active(channel)) {
        return false;
    }
    if (MUCHANNEL_NULL_EPOCH == reader->epoch ||
            has_epoch_changed(channel, reader))
    {
        res = synchronize(channel, reader);
        if (MUCHANNEL_INCOMPATIBLE_INTERFACE == res) {
            return false;
        }
    }

    serialized_copy(&channel->hdr.wc, &wc);
    return wc > reader->rc;
}
