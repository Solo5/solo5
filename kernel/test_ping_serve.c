#include "solo5.h"

extern void solo5_ping_serve(void); /* XXX */

void start_kernel(void)
{
    const char s[] = "Hello, World\n";

    solo5_console_write(s, sizeof s);

    for(;;)
        solo5_ping_serve();  /* does things if network packet comes in */
}
