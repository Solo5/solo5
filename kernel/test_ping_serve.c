#include "kernel.h"

void start_kernel(void)
{
    printf("Hello World\n");
    for(;;)
        ping_serve();  /* does things if network packet comes in */
}

