/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
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

/*
 * This file contains code derived from musl libc, licensed under the
 * following standard MIT license:
 *
 * Copyright © 2005-2014 Rich Felker, et al.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * This file currently serves dual purposes; it's used both by the Solo5
 * kernel code and the tests. The presence of __SOLO5_KERNEL__ controls
 * which mode of operation is selected.
 *
 * When adding functions to this file ensure that they are entirely
 * self-contained, or, even better, work on the TODO below instead.
 *
 * TODO: Replace this with a proper intergration of nolibc into both the
 * kernel and tests.
 */
#ifdef __SOLO5_KERNEL__
#include "kernel.h"
#endif

void *memset(void *dest, int c, size_t n)
{
    unsigned char *s = dest;

    for (; n; n--, s++) *s = c;
    return dest;
}

void *memcpy(void *restrict dest, const void *restrict src, size_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;

    for (; n; n--) *d++ = *s++;
    return dest;
}

#define WT size_t
#define WS (sizeof(WT))

void *memmove(void *dest, const void *src, size_t n)
{
    char *d = dest;
    const char *s = src;

    if (d==s) return d;
    if (s+n <= d || d+n <= s) return memcpy(d, s, n);

    if (d<s) {
        if ((uintptr_t)s % WS == (uintptr_t)d % WS) {
            while ((uintptr_t)d % WS) {
                if (!n--) return dest;
                *d++ = *s++;
            }
            for (; n>=WS; n-=WS, d+=WS, s+=WS) *(WT *)d = *(WT *)s;
        }
        for (; n; n--) *d++ = *s++;
    } else {
        if ((uintptr_t)s % WS == (uintptr_t)d % WS) {
            while ((uintptr_t)(d+n) % WS) {
                if (!n--) return dest;
                d[n] = s[n];
            }
            while (n>=WS) n-=WS, *(WT *)(d+n) = *(WT *)(s+n);
        }
        while (n) n--, d[n] = s[n];
    }

    return dest;
}

int memcmp(const void *vl, const void *vr, size_t n)
{
    const unsigned char *l=vl, *r=vr;
    for (; n && *l == *r; n--, l++, r++);
    return n ? *l-*r : 0;
}

int strcmp(const char *l, const char *r)
{
    for (; *l==*r && *l; l++, r++);
    return *(unsigned char *)l - *(unsigned char *)r;
}

char *strcpy(char *restrict dest, const char *restrict src)
{
    const unsigned char *s = (const unsigned char *)src;
    unsigned char *d = (unsigned char *)dest;
    while ((*d++ = *s++));
    return dest;
}

#define UCHAR_MAX 255
#define ALIGN (sizeof(size_t))
#define ONES ((size_t)-1/UCHAR_MAX)
#define HIGHS (ONES * (UCHAR_MAX/2+1))
#define HASZERO(x) (((x)-ONES) & ~(x) & HIGHS)

size_t strlen(const char *s)
{
    const char *a = s;
    const size_t *w;
    for (; (uintptr_t)s % ALIGN; s++) if (!*s) return s-a;
    for (w = (const void *)s; !HASZERO(*w); w++);
    for (s = (const void *)w; *s; s++);
    return s-a;
}

int isspace(int c)
{
    return c == ' ' || (unsigned)c-'\t' < 5;
}

int strncmp(const char *_l, const char *_r, size_t n)
{
    const unsigned char *l=(void *)_l, *r=(void *)_r;
    if (!n--) return 0;
    for (; *l && *r && n && *l == *r ; l++, r++, n--);
    return *l - *r;
}
