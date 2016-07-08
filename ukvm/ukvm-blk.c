#define _GNU_SOURCE
#include <sys/syscall.h>
#include <err.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "ukvm.h"
#include "ukvm_modules.h"

static struct ukvm_blkinfo blkinfo;
static char *diskfile;
static int fd;
static int afd; // for eventfd and to be used by the main poll in ukvm.c

/******************************************/
int eventfd_aio_write(int fd, int afd, u_int64_t off,
                      void *data, int len);
int eventfd_aio_read(int fd, int afd, u_int64_t off,
                     void *data, int len);
/******************************************/


static void ukvm_port_blkinfo(uint8_t *mem, void *data)
{
    uint32_t mem_off = *(uint32_t *) data;
    struct ukvm_blkinfo *info = (struct ukvm_blkinfo *) (mem + mem_off);

    info->sector_size = blkinfo.sector_size;
    info->num_sectors = blkinfo.num_sectors;
    info->rw = blkinfo.rw;
}

static void ukvm_port_blkwrite(uint8_t *mem, void *data)
{
    uint32_t mem_off = *(uint32_t *) data;
    struct ukvm_blkwrite *wr = (struct ukvm_blkwrite *) (mem + mem_off);
    int ret;

    wr->ret = -1;
    if (wr->sector < blkinfo.num_sectors) {
        ret = eventfd_aio_write(fd, afd,
                                blkinfo.sector_size * wr->sector,
                                mem + (uint64_t) wr->data, wr->len);
        wr->ret = ret;
    }
}

static void ukvm_port_blkread(uint8_t *mem, void *data)
{
    uint32_t mem_off = *(uint32_t *) data;
    struct ukvm_blkread *rd = (struct ukvm_blkread *) (mem + mem_off);
    int ret;

    rd->ret = -1;
    if (rd->sector < blkinfo.num_sectors) {
        ret = eventfd_aio_read(fd, afd,
                               blkinfo.sector_size * rd->sector,
                               mem + (uint64_t) rd->data, rd->len);
        rd->ret = ret;
    }
}

static int handle_exit(struct kvm_run *run, int vcpufd, uint8_t *mem)
{
    uint8_t *data;

    if (run->exit_reason != KVM_EXIT_IO)
        return -1;

    if (run->io.direction != KVM_EXIT_IO_OUT)
        return -1;

    data = (uint8_t *)run + run->io.data_offset;

    switch (run->io.port) {
    case UKVM_PORT_BLKINFO:
        ukvm_port_blkinfo(mem, data);
        break;
    case UKVM_PORT_BLKWRITE:
        ukvm_port_blkwrite(mem, data);
        break;
    case UKVM_PORT_BLKREAD:
        ukvm_port_blkread(mem, data);
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

/*
 * This were good at the time of 2.6.21-rc5.mm4 ...
 */
#ifndef __NR_eventfd
#if defined(__x86_64__)
#define __NR_eventfd 283
#elif defined(__i386__)
#define __NR_eventfd 323
#else
#error Cannot detect your architecture!
#endif
#endif


int eventfd(int count)
{
	return syscall(__NR_eventfd, count);
}

int eventfd_aio_setup(char *testfn, int afd);

static int setup(int vcpufd, uint8_t *mem)
{
    if (diskfile == NULL)
        return -1;

    if ((afd = eventfd(0)) == -1) {
        perror("eventfd");
        return 2;
    }

    /* set up virtual disk */
    fd = eventfd_aio_setup(diskfile, afd);
    printf("afd=%d fd=%d\n", afd, fd);

    blkinfo.sector_size = 512;
    blkinfo.num_sectors = lseek(fd, 0, SEEK_END) / 512;
    blkinfo.rw = 1;

    printf("Providing disk: %ld sectors @ %d = %ld bytes\n",
           blkinfo.num_sectors, blkinfo.sector_size,
           blkinfo.num_sectors * blkinfo.sector_size);

    return 0;
}

static int get_fd(void)
{
    return afd;
}

static char *usage(void)
{
    return "--disk=IMAGE (file exposed to the unikernel as a raw block device)";
}


static int handle_exit_cleanup(void)
{
    uint64_t eval = 0;
    if (read(afd, &eval, sizeof(eval)) != sizeof(eval))
	perror("read");
    //fprintf(stdout, "done! %llu\n", (unsigned long long) eval);
    return 0;
}

struct ukvm_module ukvm_blk = {
    .get_fd = get_fd,
    .handle_exit = handle_exit,
    .handle_cmdarg = handle_cmdarg,
    .handle_exit_cleanup = handle_exit_cleanup,
    .setup = setup,
    .usage = usage
};

