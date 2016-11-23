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

/*
 * Verify that global variables are initialised and writable. (See #73)
 */

char s[] = "RUCCESS\n";

void do_test(void)
{
    s[0]++;
    puts(s);
}

int solo5_app_main(char *cmdline __attribute__((unused)))
{
    puts("\n**** Solo5 standalone test_globals ****\n\n");

    do_test();

    return 0;
}
