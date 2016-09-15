#include "kernel.h"

/*
 * These functions deliberately do not call printf() or malloc() in order to
 * abort as quickly as possible without triggering further errors.
 */

static void puts(const char *s)
{
    (void)low_level_puts(s, strlen(s));
}

void _assert_fail(const char *file, const char *line, const char *e)
{
    puts("ABORT: ");
    puts(file);
    puts(":");
    puts(line);
    puts(": Assertion `");
    puts(e);
    puts("' failed\nHalted\n");
    cpu_halt();
}

void _abort(const char *file, const char *line, const char *s)
{
    puts("ABORT: ");
    puts(file);
    puts(":");
    puts(line);
    puts(": ");
    puts(s);
    puts("\nHalted\n");
    cpu_halt();
}
