#include "solo5.h"

static size_t strlen(const char *s)
{
    size_t len = 0;

    while (*s++)
        len += 1;
    return len;
}

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

int solo5_app_main(char *cmdline)
{
    puts("\n**** Solo5 standalone test_hello ****\n\n");
    puts("Hello, World\nCommand line is: '");

    size_t len = 0;
    char *p = cmdline;

    while (*p++)
        len++;
    solo5_console_write(cmdline, len);

    puts("'\n");
    puts("SUCCESS\n");

    return 0;
}
