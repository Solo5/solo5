#include <err.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "ukvm-private.h"
#include "ukvm-modules.h"
#include "ukvm.h"

static struct ukvm_blkinfo blkinfo;
static char *diskfile;
static int diskfd;

static void ukvm_port_blkinfo(uint8_t *mem, uint64_t paddr)
{
    GUEST_CHECK_PADDR(paddr, GUEST_SIZE, sizeof (struct ukvm_blkinfo));
    struct ukvm_blkinfo *info = (struct ukvm_blkinfo *)(mem + paddr);

    info->sector_size = blkinfo.sector_size;
    info->num_sectors = blkinfo.num_sectors;
    info->rw = blkinfo.rw;
}

static void ukvm_port_blkwrite(uint8_t *mem, uint64_t paddr)
{
    GUEST_CHECK_PADDR(paddr, GUEST_SIZE, sizeof (struct ukvm_blkwrite));
    struct ukvm_blkwrite *wr = (struct ukvm_blkwrite *)(mem + paddr);
    int ret;

    if (wr->sector >= blkinfo.num_sectors) {
        wr->ret = -1;
        return;
    }

    ret = lseek(diskfd, blkinfo.sector_size * wr->sector, SEEK_SET);
    assert(ret != (off_t)-1);
    GUEST_CHECK_PADDR(wr->data, GUEST_SIZE, wr->len);
    ret = write(diskfd, mem + wr->data, wr->len);
    assert(ret == wr->len);
    wr->ret = 0;
}

static void ukvm_port_blkread(uint8_t *mem, uint64_t paddr)
{
    GUEST_CHECK_PADDR(paddr, GUEST_SIZE, sizeof (struct ukvm_blkread));
    struct ukvm_blkread *rd = (struct ukvm_blkread *)(mem + paddr);
    int ret;

    if (rd->sector >= blkinfo.num_sectors) {
        rd->ret = -1;
        return;
    }

    ret = lseek(diskfd, blkinfo.sector_size * rd->sector, SEEK_SET);
    assert(ret != (off_t)-1);
    GUEST_CHECK_PADDR(rd->data, GUEST_SIZE, rd->len);
    ret = read(diskfd, mem +  rd->data, rd->len);
    assert(ret == rd->len);
    rd->ret = 0;
}

static int handle_exit(struct kvm_run *run, int vcpufd, uint8_t *mem)
{
    if ((run->exit_reason != KVM_EXIT_IO) ||
        (run->io.direction != KVM_EXIT_IO_OUT) ||
        (run->io.size != 4))
        return -1;

    uint64_t paddr =
        GUEST_PIO32_TO_PADDR((uint8_t *)run + run->io.data_offset);

    switch (run->io.port) {
    case UKVM_PORT_BLKINFO:
        ukvm_port_blkinfo(mem, paddr);
        break;
    case UKVM_PORT_BLKWRITE:
        ukvm_port_blkwrite(mem, paddr);
        break;
    case UKVM_PORT_BLKREAD:
        ukvm_port_blkread(mem, paddr);
        break;
    default:
        return -1;
    }

    return 0;
}

static int handle_cmdarg(char *cmdarg)
{
    if (strncmp("--disk=", cmdarg, 7))
        return -1;
    diskfile = cmdarg + 7;

    return 0;
}

static int setup(int vcpufd, uint8_t *mem)
{
    if (diskfile == NULL)
        return -1;

    /* set up virtual disk */
    diskfd = open(diskfile, O_RDWR);
    if (diskfd == -1)
        err(1, "Could not open disk: %s", diskfile);

    blkinfo.sector_size = 512;
    blkinfo.num_sectors = lseek(diskfd, 0, SEEK_END) / 512;
    blkinfo.rw = 1;

    return 0;
}

static int get_fd(void)
{
    return 0; /* no fd for poll to sleep on (synchronous) */
}

static char *usage(void)
{
    return "--disk=IMAGE (file exposed to the unikernel as a raw block device)";
}

struct ukvm_module ukvm_blk = {
    .get_fd = get_fd,
    .handle_exit = handle_exit,
    .handle_cmdarg = handle_cmdarg,
    .setup = setup,
    .usage = usage,
    .name = "blk"
};

