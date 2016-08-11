#include "kernel.h"

/* ukvm net interface */
int solo5_net_write_sync(_UNUSED solo5_device * dev,
                         uint8_t *data, int n)
{
    volatile struct ukvm_netwrite wr;

    wr.data = data;
    wr.len = n;
    wr.ret = 0;

    /* TODO: get the port associated with device * dev */

    outl(UKVM_PORT_NETWRITE, ukvm_ptr(&wr));
    cc_barrier();

    return wr.ret;
}

int solo5_net_read_sync(_UNUSED solo5_device * dev,
                        uint8_t *data, int *n)
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
char *solo5_net_mac_str(_UNUSED solo5_device * dev)
{
    volatile struct ukvm_netinfo info;

    outl(UKVM_PORT_NETINFO, ukvm_ptr(&info));
    cc_barrier();

    memcpy(mac_str, (void *)&info, 18);
    return mac_str;
}

/* ukvm block interface */
int solo5_blk_write_sync(_UNUSED solo5_device *dev, uint64_t sec, uint8_t *data, int n)
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

int solo5_blk_read_sync(_UNUSED solo5_device *dev, uint64_t sec, uint8_t *data, int *n)
{
    volatile struct ukvm_blkread rd;

    rd.sector = sec;
    rd.data = data;
    rd.len = *n;
    rd.ret = 0;

    outl(UKVM_PORT_BLKREAD, ukvm_ptr(&rd));
    cc_barrier();

    *n = rd.len;
    return rd.ret;
}

int solo5_blk_sector_size(_UNUSED solo5_device *dev)
{
    volatile struct ukvm_blkinfo info;

    /* TODO: get the port associated with device * dev */

    outl(UKVM_PORT_BLKINFO, ukvm_ptr(&info));
    cc_barrier();

    return info.sector_size;
}

uint64_t solo5_blk_sectors(_UNUSED solo5_device *dev)
{
    volatile struct ukvm_blkinfo info;

    /* TODO: get the port associated with device * dev */

    outl(UKVM_PORT_BLKINFO, ukvm_ptr(&info));
    cc_barrier();

    return info.num_sectors;
}

int solo5_blk_rw(_UNUSED solo5_device *dev)
{
    volatile struct ukvm_blkinfo info;

    outl(UKVM_PORT_BLKINFO, ukvm_ptr(&info));
    cc_barrier();

    return info.rw;
}
