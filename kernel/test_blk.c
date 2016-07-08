#include "solo5.h"

#define SECTOR_SIZE	512
static uint8_t sector_write[SECTOR_SIZE];
static uint8_t sector_read[SECTOR_SIZE];

int solo5_app_main(char *cmdline __attribute__((unused)))
{
    int n = SECTOR_SIZE;
    int i;
    short events[SOLO5_NUM_DEVICES];
    short revents[SOLO5_NUM_DEVICES];
    solo5_device *disk = solo5_get_first_disk();

    for (i = 0; i < 10; i++)
        sector_write[i] = '0' + i;
    sector_write[10] = '\n';

    solo5_blk_write_sync(disk, 0, sector_write, SECTOR_SIZE);
    solo5_blk_read_sync(disk, 0, sector_read, &n);
    solo5_console_write((const char *) sector_read, 11);

    // this one should timeout after a second as there are no events
    events[disk->poll_event_idx] = SOLO5_POLL_IO_READY;
    solo5_poll(solo5_clock_monotonic() + 1e9, events, revents);

    return 0;
}
