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

#include "../kernel.h"
#include "writer.h"
#include "util.h"

void muen_channel_init_writer(struct muchannel *channel, const uint64_t protocol,
                  const uint64_t element_size, const uint64_t channel_size,
                  const uint64_t epoch)
{
    uint64_t data_size;

    muen_channel_deactivate(channel);
    memset(channel, 0, sizeof(struct muchannel));
    data_size = channel_size - sizeof(struct muchannel_header);
    memset(channel->data, 0, data_size);

    channel->hdr.transport = SHMSTREAM20;
    channel->hdr.protocol  = protocol;
    channel->hdr.size      = element_size;
    channel->hdr.elements  = data_size / element_size;
    channel->hdr.wsc       = 0;
    channel->hdr.wc        = 0;

    serialized_copy(&epoch, &channel->hdr.epoch);
}

void muen_channel_deactivate(struct muchannel *channel)
{
    channel->hdr.epoch = MUCHANNEL_NULL_EPOCH;
    cc_barrier();
}

void muen_channel_write(struct muchannel *channel, const void * const element)
{
    uint64_t wc, pos, size;

    size = channel->hdr.size;
    wc = channel->hdr.wc;
    pos = wc % channel->hdr.elements;

    wc++;

    serialized_copy(&wc, &channel->hdr.wsc);
    memcpy(channel->data + pos * size, element, size);
    serialized_copy(&wc, &channel->hdr.wc);
}
