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
#include "sinfo.h"
#include "writer.h"

#define DEBUGLOG_PROTO 0xf00789094b6f70cfUL

struct log_msg {
    uint64_t timestamp;
    char data[56];
} __attribute__((packed));

static struct muchannel *channel_out;
static struct log_msg msg_buffer;
static int msg_index;

static void clear_buffer()
{
    msg_index = 0;
    memset(&msg_buffer, 0, sizeof(struct log_msg));
}

static void flush()
{
    msg_buffer.timestamp = muen_get_sched_start();
    muen_channel_write(channel_out, &msg_buffer);
    clear_buffer();
}

int platform_puts(const char *buf, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        if (buf[i] && buf[i] != 0x0d)
        {
            msg_buffer.data[msg_index] = buf[i];
            if (msg_index == 55 || buf[i] == 0x0a)
                flush();
            else
                msg_index++;
        }
    }

    return n;
}

void solo5_console_write(const char *buf, size_t size)
{
    (void)platform_puts(buf, size);
}

void console_init(void)
{
    struct muen_channel_info channel;
    uint64_t epoch;

    if (!muen_get_channel_info("debuglog", &channel)) {
        return;
    }

    clear_buffer();
    epoch        = muen_get_sched_start();
    channel_out  = (struct muchannel *)(channel.address);
    muen_channel_init_writer(channel_out, DEBUGLOG_PROTO, sizeof(struct log_msg),
            channel.size, epoch);
    log(INFO, "Solo5: Console: Muen Channel @ 0x%lx, size 0x%lx, epoch 0x%lx\n",
        channel.address, channel.size, epoch);
}
