#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

void abort(void) __attribute__((noreturn));
void exit(int) __attribute__((noreturn));
void *malloc(size_t);
void free(void *);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
char *getenv(const char *);
int system(const char *);
double strtod(const char *, char **);
long strtol(const char *, char **, int);

#endif
