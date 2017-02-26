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

#ifndef MUEN_CHANNEL_WRITER_H
#define MUEN_CHANNEL_WRITER_H

#include "channel.h"

/**
 * Initialize channel with given parameters.
 */
void muen_channel_init_writer(struct muchannel *channel, const uint64_t protocol,
                  const uint64_t element_size, const uint64_t channel_size,
                  const uint64_t epoch);

/**
 * Deactivate channel.
 */
void muen_channel_deactivate(struct muchannel *channel);

/**
 * Write element to given channel.
 */
void muen_channel_write(struct muchannel *channel, const void * const element);

#endif
