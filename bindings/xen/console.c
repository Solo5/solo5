/*
 * Copyright (c) 2015-2020 Contributors as noted in the AUTHORS file
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
 * Based on XTF console.c, which is:
 *
 * The Xen Test Framework is licenced under the BSD 2-clause license
 *
 * Copyright (c) 2014,2015 Citrix Systems Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings.h"

#include "hypercall.h"
#include "xen/hvm/params.h"
#include "xen/io/console.h"

/* Use XTF terminology here. */
#define LOAD_ACQUIRE(p)       (__atomic_load_n((p), __ATOMIC_ACQUIRE))
#define STORE_RELEASE(p, val) (__atomic_store_n((p), val, __ATOMIC_RELEASE))
#define ACCESS_ONCE(x)        (*(volatile __typeof__(x) *)&(x))

static bool console_initialised = false;
static struct xencons_interface *console_ring;
static evtchn_port_t console_evtchn;

/*
 * Write some data into the console ring, taking care not to overflow the ring.
 */
static size_t console_write_some(const char *buf, size_t len)
{
    size_t s = 0;
    XENCONS_RING_IDX cons = LOAD_ACQUIRE(&console_ring->out_cons),
                     prod = console_ring->out_prod;
    bool wrote_cr = false;

    while ((s < len) && ((prod - cons) < sizeof(console_ring->out))) {
        /* Translate LF to CRLF while we're here. Could cause a CR to be output
         * twice if we overrun the ring, too bad. */
        if (buf[s] == '\n') {
            if (!wrote_cr) {
                console_ring->out[prod++ & (sizeof(console_ring->out) - 1)] \
                    = '\r';
                wrote_cr = true;
                continue;
            }
            else {
                wrote_cr = false;
            }
        }
        console_ring->out[prod++ & (sizeof(console_ring->out) - 1)] = buf[s++];
    }

    STORE_RELEASE(&console_ring->out_prod, prod);
    return s;
}

/*
 * Write data into the console ring, synchronously waiting for data to be
 * consumed if the ring would overflow.
 */
void console_write(const char *buf, size_t len)
{
    if (!console_initialised)
        return;

    size_t written = 0;
    XENCONS_RING_IDX cons = LOAD_ACQUIRE(&console_ring->out_cons);

    do
    {
        /* Try and put some data into the ring. */
        written = console_write_some(&buf[written], len - written);

        /* Signal xenconsoled that new data is available. */
        hypercall_evtchn_send(console_evtchn);

        /*
         * If we have more to write, the ring must have filled up.  Wait for
         * more space.
         */
        if (written < len)
        {
            while (ACCESS_ONCE(console_ring->out_cons) == cons)
                hypercall_yield();
        }

    } while (written < len);

#if 0 /* For debugging only, no need to wait synchronously here. */
    /* Wait for xenconsoled to consume all the data we gave. */
    while (ACCESS_ONCE(console_ring->out_cons) != console_ring->out_prod)
        hypercall_yield();
#endif
}

void console_init(void)
{
    int rc;
    uint64_t raw_pfn, raw_evtchn;

    assert(!console_initialised);
    rc = hypercall_hvm_get_param(HVM_PARAM_CONSOLE_PFN, &raw_pfn);
    assert(rc == 0);
    rc = hypercall_hvm_get_param(HVM_PARAM_CONSOLE_EVTCHN, &raw_evtchn);
    assert(rc == 0);

    console_ring = (struct xencons_interface *)(raw_pfn << PAGE_SHIFT);
    console_evtchn = raw_evtchn;
    console_initialised = true;
    /*
     * This can't be DEBUG since cmdline_parse() has not been called yet.
     * Keep it at INFO for now, as it's a good indication that things are
     * up and running early in the boot process.
     */
    log(INFO, "Solo5: Xen console: port 0x%x, ring @0x%p\n", console_evtchn,
            console_ring);
}
