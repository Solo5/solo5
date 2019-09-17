/*
 * Copyright (c) 2017 Contributors as noted in the AUTHORS file
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

#include "bindings.h"
#include "muen-net.h"

void solo5_yield(solo5_time_t deadline, solo5_handle_set_t *ready_set)
{
    solo5_handle_set_t tmp_ready_set = 0;

    do {
        for (solo5_handle_t i = 0U; i < MFT_MAX_ENTRIES; ++i) {
            if (muen_net_pending_data(i))
                tmp_ready_set |= 1UL << i;
        }

        if (tmp_ready_set > 0)
            break;

        __asm__ __volatile__("pause");
    } while (solo5_clock_monotonic() < deadline);

    if (ready_set)
        *ready_set = tmp_ready_set;
}
