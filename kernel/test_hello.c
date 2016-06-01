#include "solo5.h"

#define UNUSED(x) (void)(x)

int start_kernel(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    const char s[] = "Hello, World\n";

    solo5_console_write(s, sizeof s);

    return 0;
}

