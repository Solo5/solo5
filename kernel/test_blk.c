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
    short events[SOLO5_NUM_DEVICES];
    short revents[SOLO5_NUM_DEVICES];
    struct solo5_device_t *disk;
    solo5_request req;
    int idx_first_blk = solo5_get_first_disk()->poll_event_idx;

    memset(events, 0, SOLO5_NUM_DEVICES * sizeof(events));
    events[idx_first_blk] = SOLO5_POLLIN;

    disk = solo5_get_first_disk();

    solo5_console_write(s, sizeof(s));

    // sync test
    memset(blk_sector, 0, 512);
    for (i = 0; i < 10; i++)
        blk_sector[i] = '0' + i;
    // wr
    disk->sync_write(0, blk_sector, 512);
    memset(blk_sector, 0, 512);
    n = 512;
    // rd
    disk->sync_read(0, blk_sector, &n);
    solo5_console_write((const char *) blk_sector, 10);
    solo5_console_write("\n", 1);
    // this one should timeout after a second as there are no more events
    solo5_poll(solo5_clock_monotonic() + 1e9, events, revents);

    // async test
    memset(blk_sector, 0, 512);
    for (i = 0; i < 10; i++)
        blk_sector[i] = '0' + i;
    // wr
    req = disk->async_write(0, blk_sector, 512);
    solo5_poll(solo5_clock_monotonic() + 1e10, events, revents);
    disk->async_write_result(req, &n);

    memset(blk_sector, 0, 512);
    n = 512;
    req = disk->async_read(0, &n);
    solo5_poll(solo5_clock_monotonic() + 1e10, events, revents);
    // rd
    disk->async_read_result(req, blk_sector, &n);
    solo5_console_write((const char *) blk_sector, 10);
    solo5_console_write("\n", 1);

    return 0;
}
