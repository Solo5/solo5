/* Copyright (c) 2015, IBM 
 * Author(s): Dan Williams <djwillia@us.ibm.com> 
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

#include "kernel.h"

void *memset(void *ptr, uint8_t c, size_t size) {
    size_t i;
    for ( i = 0; i < size; i++ ) 
        ((uint8_t *)ptr)[i] = c;

    return ptr;
}

void *memcpy(void *dst, const void *src, size_t size) {
    size_t i;
    for ( i = 0; i < size; i++ ) 
        ((uint8_t *)dst)[i] = ((uint8_t *)src)[i];
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *tmp = (uint8_t *)malloc(n);
    memcpy(tmp, src, n);
    memcpy(dst, tmp, n);
    free(tmp);

    return dst;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    size_t i;

    for ( i = 0; i < n; i++ ) {
        if (((uint8_t *)s1)[i] < ((uint8_t *)s2)[i])
            return -1;
        if (((uint8_t *)s1)[i] > ((uint8_t *)s2)[i])
            return 1;
    }
    return 0;
}

int strcmp(const char *s1, const char *s2){
    while (s1 != 0) {
        if ((uint8_t)*s1 > (uint8_t)*s2)
            return 1;
        if ((uint8_t)*s1 < (uint8_t)*s2)
            return -1;
        s1++;
        s2++;
    }

    return (*s2) ? -1 : 0;
}

char *strcpy(char *dst, const char *src) {
    size_t i = 0;
    do {
        dst[i] = src[i];
    } while (src[i++] != 0);

    return dst;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;

    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for ( ; i < n; i++)
        dest[i] = '\0';

    return dest;
}

size_t strlen(const char *s) {
    int n = 0;

    while (*s) {
        n++;
        s++;
    }

    return n;
}

int isdigit(int c) {
    /* not a character */
    if (( c >= 128 ) || ( c < 0 ))
        return 0;

    if (((char)c >= '0') && ((char)c <= '9'))
        return 1;

    return 0;
}

int toupper(int c) {
    if (((char)c >= 'a') && ((char)c <= 'z'))
        return c - ('a' - 'A');

    return c;
}

int isxdigit(int c) {

    /* not a character */
    if (( c >= 128 ) || ( c < 0 ))
        return 0;

    if (((char)c >= '0') && ((char)c <= '9'))
        return 1;

    if (((char)c >= 'a') && ((char)c <= 'f'))
        return 1;

    if (((char)c >= 'A') && ((char)c <= 'F'))
        return 1;

    return 0;
}

/**  COPIED FROM MINIOS:
 * simple_strtoul - convert a string to an unsigned long
 * @cp: The start of the string
 * @endp: A pointer to the end of the parsed string will be placed here
 * @base: The number base to use
 */
unsigned long simple_strtoul(const char *cp,char **endp,unsigned int base)
{
    unsigned long result = 0,value;

    if (!base) {
        base = 10;
        if (*cp == '0') {
            base = 8;
            cp++;
            if ((*cp == 'x') && isxdigit(cp[1])) {
                cp++;
                base = 16;
            }
        }
    }
    while (isxdigit(*cp) &&
           (value = isdigit(*cp) ? *cp-'0' : toupper(*cp)-'A'+10) < base) {
        result = result*base + value;
        cp++;
    }
    if (endp)
        *endp = (char *)cp;
    return result;
}

int abs(int j) {
    return (j > 0) ? j : (-1 * j);
}
long int labs(long int j) {
    return (j > 0) ? j : (-1 * j);
}
long long int llabs(long long int j) {
    return (j > 0) ? j : (-1 * j);
}

