#include "solo5.h"

#define UNUSED(x) (void)(x)

extern void solo5_ping_serve(void); /* XXX */

int start_kernel(int argc, char  **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    const char s[] = "Hello, World\n";

    solo5_console_write(s, sizeof s);

    for(;;)
        solo5_ping_serve();  /* does things if network packet comes in */

    return 0;
}
