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

#include "bindings.h"

/* _stdata is the address begining of the "not bss" thread variables block */
extern char _stdata[];
#define TDATA ((void*)_stdata)

/* _ltdata and _ltbss are the lengths of the tdata and tbss blocks */
extern char _ltdata[], _ltbss[];
#define LTDATA ((size_t)_ltdata)
#define LTBSS ((size_t)_ltbss)

/* The following define the overhead to the tdata and tbss blocks
 * It's processor related
 */
#if defined(__x86_64__) || defined(__powerpc64__)
    /* Variant II */
    struct tcb {
        void* tp;
    };

    #define PPC64_TLS_OFFSET 0x7000

#elif defined(__aarch64__)
    /* Variant I */
    struct tcb {
        void* tp;
        void* pad;
    };

#else
#error Unsupported architecture
#endif

size_t solo5_tls_size()
{
    return (LTDATA + LTBSS + sizeof(struct tcb));
}

uintptr_t solo5_tls_tp_offset(uintptr_t tls)
{
    uintptr_t tp;
#if defined(__powerpc64__)
    tp = PPC64_TLS_OFFSET + tls;
#elif defined(__x86_64__)
    tp = tls + LTDATA + LTBSS;
#elif defined(__aarch64__)
    tp = tls;
#else
#error Unsupported architecture
#endif

    return tp;
}

uintptr_t _solo5_tls_data_offset(uintptr_t tls)
{
    uintptr_t data;
#if defined(__x86_64__) || defined(__powerpc64__)
    data = tls;
#elif defined(__aarch64__)
    data = data + sizeof(struct tcb);
#else
#error Unsupported architecture
#endif

    return data;
}

solo5_result_t solo5_tls_init(uintptr_t tls)
{
	if ((void*)tls == NULL) return SOLO5_R_EINVAL;

    /* set tp at its proper place in the TLS block */
    uintptr_t *tmp = (uintptr_t*)solo5_tls_tp_offset(tls);
    *tmp = *tmp;

    /* copy the .tdata values */
    memcpy((void*)_solo5_tls_data_offset(tls), TDATA, LTDATA);

    return SOLO5_R_OK;
}

solo5_result_t solo5_set_tls_base(uintptr_t base)
{
    int rc = platform_set_tls_base(base);
    return (rc == 0) ? SOLO5_R_OK : SOLO5_R_EINVAL;
}
