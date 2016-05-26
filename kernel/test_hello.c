#include "solo5.h"

void start_kernel(void)
{
    const char s[] = "Hello, World\n";

    solo5_console_write(s, sizeof s);
}

