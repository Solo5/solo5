#include "solo5.h"

static uint8_t blk_sector[512];

void *memset(void *s, int c, size_t n)
{
    unsigned char* p=s;
    while(n--)
        *p++ = (unsigned char)c;
    return s;
}

int solo5_app_main(char *cmdline __attribute__((unused)))
{
    const char s[] = "test_blk\n";
    int n = 512, i;
    short events[3] = {1, 0, 0}; // XXX channel 0 is hardcoded
    short revents[3];

    solo5_console_write(s, sizeof(s));

    // sync test
    memset(blk_sector, 0, 512);
    for (i = 0; i < 10; i++)
        blk_sector[i] = '0' + i;
    solo5_blk_write_sync(0, blk_sector, 512);
    memset(blk_sector, 0, 512);
    n = 512;
    solo5_blk_read_sync(0, blk_sector, &n);
    solo5_console_write((const char *) blk_sector, 10);
    solo5_console_write("\n", 1);

    // this one should timeout after a second as there are no more events
    solo5_poll(solo5_clock_monotonic() + 1e9, events, revents);

    // async test
    memset(blk_sector, 0, 512);
    for (i = 0; i < 10; i++)
        blk_sector[i] = '0' + i;
    solo5_blk_write_async(0, blk_sector, 512);
    solo5_poll(solo5_clock_monotonic() + 1e10, events, revents);
    memset(blk_sector, 0, 512);
    n = 512;
    solo5_blk_read_async_submit(0, &n);
    solo5_poll(solo5_clock_monotonic() + 1e10, events, revents);
    solo5_blk_read_async_complete(blk_sector, &n);
    solo5_console_write((const char *) blk_sector, 10);
    solo5_console_write("\n", 1);

    return 0;
}
