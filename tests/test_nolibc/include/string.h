#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

int memcmp(const void *, const void *, size_t);
void *memchr(const void *, int, size_t);
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
int strcmp(const char *, const char *);
size_t strlen(const char *);
char *strerror(int);
/*
 * The following definitions are not required by the OCaml runtime, but are
 * needed to build the freestanding version of GMP used by Mirage.
 */
char *strncpy(char *, const char *, size_t);
char *strchr(const char *, int);

#endif
