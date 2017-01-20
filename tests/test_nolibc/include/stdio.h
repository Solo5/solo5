#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stddef.h>

struct _FILE;
typedef struct _FILE {
    size_t (*write)(struct _FILE *f, const char *, size_t);
    char *wpos;
    char *wend;
} FILE;
int fflush(FILE *);
int fprintf(FILE *, const char *, ...);
int printf(const char *, ...);
int rename(const char *, const char *);
extern FILE *stdout;
extern FILE *stderr;
int sscanf(const char *, const char *, ...);
int snprintf(char *, size_t, const char *, ...);
int vfprintf(FILE *, const char *, va_list);
int vsnprintf(char *, size_t, const char *, va_list);
/*
 * The following definitions are not required by the OCaml runtime, but are
 * needed to build the freestanding version of GMP used by Mirage.
 */
#define EOF (-1)
extern FILE *stdin;
size_t fread(void *, size_t, size_t, FILE *);
int getc(FILE *);
int ungetc(int, FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);
int fputc(int, FILE *);
int putc(int, FILE *);
int ferror(FILE *);

#endif
