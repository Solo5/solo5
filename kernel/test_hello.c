#include "solo5.h"

#define UNUSED(x) (void)(x)

int start_kernel(char *cmdline)
{
    const char s[] = "Hello, World\nCommand line is: ";

    solo5_console_write(s, sizeof s);

    size_t len = 0;
    char *p = cmdline;
    while (*p++)
        len++;
    solo5_console_write(cmdline, len);
    solo5_console_write("\n", 1);

    return 0;
}

