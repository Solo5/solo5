#include "kernel.h"

/* ukvm net interface */
int solo5_net_write_sync(uint8_t *data, int n)
{
    volatile struct ukvm_netwrite wr;

    wr.data = data;
    wr.len = n;
    wr.ret = 0;

    outl(UKVM_PORT_NETWRITE, ukvm_ptr(&wr));
    cc_barrier();

    return wr.ret;
}

int solo5_net_read_sync(uint8_t *data, int *n)
{
    volatile struct ukvm_netread rd;

    rd.data = data;
    rd.len = *n;
    rd.ret = 0;

    outl(UKVM_PORT_NETREAD, ukvm_ptr(&rd));
    cc_barrier();

    *n = rd.len;

    return rd.ret;
}

static char mac_str[18];
char *solo5_net_mac_str(void)
{
    volatile struct ukvm_netinfo info;

    outl(UKVM_PORT_NETINFO, ukvm_ptr(&info));
    cc_barrier();

    memcpy(mac_str, (void *)&info, 18);
    return mac_str;
}

/* ukvm block interface */
int solo5_blk_write_async(uint64_t sec, uint8_t *data, int n)
{
    volatile struct ukvm_blkwrite wr;

    wr.sector = sec;
    wr.data = data;
    wr.len = n;
    wr.ret = 0;

    outl(UKVM_PORT_BLKWRITE, ukvm_ptr(&wr));
    cc_barrier();

    return wr.ret;
}
int solo5_blk_write_sync(uint64_t sec, uint8_t *data, int n)
{
    int ret = solo5_blk_write_async(sec, data, n);
    short events[3] = {SOLO5_POLLIN, 0, 0};
    solo5_poll(solo5_clock_monotonic() + 1e9, events, NULL);
    return ret;
}


// FIXME: the one and only request
// this is our "ring" of requests. With size of ring = 1
static volatile struct ukvm_blkread only_one_rd;

int solo5_blk_read_async_submit(uint64_t sec, int *n)
{
    only_one_rd.sector = sec;
    only_one_rd.data = malloc(*n);
    only_one_rd.len = *n;
    only_one_rd.ret = 0;

    outl(UKVM_PORT_BLKREAD, ukvm_ptr(&only_one_rd));
    cc_barrier();

    return 0;
}

int solo5_blk_read_async_complete(uint8_t *data, int *n)
{
    // TODO: avoid this copy by having submit get the *data arg
    *n = only_one_rd.len;
    memcpy(data, only_one_rd.data, *n);
    return only_one_rd.ret;
}

int solo5_blk_read_sync(uint64_t sec, uint8_t *data, int *n)
{
    int ret = solo5_blk_read_async_submit(sec, n);
    // TODO: our "channel" is 0
    short events[3] = {SOLO5_POLLIN, 0, 0};
    short revents[3];
    solo5_poll(solo5_clock_monotonic() + 1e9, events, revents);
    printf("revents: %d %d %d\n", events[0], events[1], events[2]);
    assert(events[0] == SOLO5_POLLIN);
    ret = solo5_blk_read_async_complete(data, n);
    return ret;
}

int solo5_blk_sector_size(void)
{
    volatile struct ukvm_blkinfo info;

    outl(UKVM_PORT_BLKINFO, ukvm_ptr(&info));
    cc_barrier();

    return info.sector_size;
}

uint64_t solo5_blk_sectors(void)
{
    volatile struct ukvm_blkinfo info;

    outl(UKVM_PORT_BLKINFO, ukvm_ptr(&info));
    cc_barrier();

    return info.num_sectors;
}

int solo5_blk_rw(void)
{
    volatile struct ukvm_blkinfo info;

    outl(UKVM_PORT_BLKINFO, ukvm_ptr(&info));
    cc_barrier();

    return info.rw;
}
