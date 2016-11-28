#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include <sys/select.h>

/* for net */
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <err.h>

#include "ukvm-private.h"
#include "ukvm-modules.h"
#include "ukvm.h"

static char *netiface;
static int netfd;
static struct ukvm_netinfo netinfo;

/*
 * Attach to an existing TAP interface named 'dev'.
 *
 * This function abstracts away the horrible implementation details of the
 * Linux tun API by ensuring (as much as is possible) success if and only if
 * the TAP device named 'dev' already exists.
 *
 * Returns -1 and an appropriate errno on failure (ENODEV if the device does
 * not exist), and the tap device file descriptor on success.
 */
static int tap_attach(const char *dev)
{
    struct ifreq ifr;
    int fd, err;

    fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK);
    if (fd == -1)
        return -1;

    /*
     * Initialise ifr for TAP interface.
     */
    memset(&ifr, 0, sizeof(ifr));
    /*
     * TODO: IFF_NO_PI may silently truncate packets on read().
     */
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    if (strlen(dev) > IFNAMSIZ) {
        errno = EINVAL;
        return -1;
    }
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);

    /*
     * Try to create OR attach to an existing device. The Linux API has no way
     * to differentiate between the two, but see below.
     */
    if (ioctl(fd, TUNSETIFF, (void *)&ifr) == -1) {
        err = errno;
        close(fd);
        /*
         * If we got back EPERM then the device would have been created but we
         * don't have permission to do that. Translate that to ENODEV since
         * that's what it means for our purposes.
         */
        if (err == EPERM)
            errno = ENODEV;
        else
            errno = err;
        return -1;
    }
    /*
     * If we got back a different device than the one requested, e.g. because
     * the caller mistakenly passed in '%d' (yes, that's really in the Linux API)
     * then fail.
     */
    if (strncmp(ifr.ifr_name, dev, IFNAMSIZ) != 0) {
        close(fd);
        errno = ENODEV;
        return -1;
    }

    /*
     * Attempt a zero-sized write to the device. If the device was freshly
     * created (as opposed to attached to an existing one) this will fail with
     * EIO. Ignore any other error return since that may indicate the device
     * is up.
     *
     * If this check produces a false positive then caller's later writes to fd
     * will fail with EIO, which is not great but at least we tried.
     */
    char buf[1] = { 0 };
    if (write(fd, buf, 0) == -1 && errno == EIO) {
        close(fd);
        errno = ENODEV;
        return -1;
    }

    return fd;
}

static void ukvm_port_netinfo(uint8_t *mem, uint64_t paddr)
{
    GUEST_CHECK_PADDR(paddr, GUEST_SIZE, sizeof (struct ukvm_netinfo));
    struct ukvm_netinfo *info = (struct ukvm_netinfo *)(mem + paddr);

    memcpy(info->mac_str, netinfo.mac_str, sizeof(netinfo.mac_str));
}

static void ukvm_port_netwrite(uint8_t *mem, uint64_t paddr)
{
    GUEST_CHECK_PADDR(paddr, GUEST_SIZE, sizeof (struct ukvm_netwrite));
    struct ukvm_netwrite *wr = (struct ukvm_netwrite *)(mem + paddr);
    int ret;

    GUEST_CHECK_PADDR(wr->data, GUEST_SIZE, wr->len);
    ret = write(netfd, mem + wr->data, wr->len);
    assert(wr->len == ret);
    wr->ret = 0;
}

static void ukvm_port_netread(uint8_t *mem, uint64_t paddr)
{
    GUEST_CHECK_PADDR(paddr, GUEST_SIZE, sizeof (struct ukvm_netread));
    struct ukvm_netread *rd = (struct ukvm_netread *)(mem + paddr);
    int ret;

    GUEST_CHECK_PADDR(rd->data, GUEST_SIZE, rd->len);
    ret = read(netfd, mem + rd->data, rd->len);
    if ((ret == 0) ||
        (ret == -1 && errno == EAGAIN)) {
        rd->ret = -1;
        return;
    }
    assert(ret > 0);
    rd->len = ret;
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
    case UKVM_PORT_NETINFO:
        ukvm_port_netinfo(mem, paddr);
        break;
    case UKVM_PORT_NETWRITE:
        ukvm_port_netwrite(mem, paddr);
        break;
    case UKVM_PORT_NETREAD:
        ukvm_port_netread(mem, paddr);
        break;
    default:
        return -1;
    }

    return 0;
}

static int handle_cmdarg(char *cmdarg)
{
    if (strncmp("--net=", cmdarg, 6))
        return -1;
    netiface = cmdarg + 6;
    return 0;
}

static int setup(int vcpufd, uint8_t *mem)
{
    if (netiface == NULL)
        return -1;

    /* attach to requested tap interface */
    netfd = tap_attach(netiface);
    if (netfd < 0) {
        err(1, "Could not attach interface: %s", netiface);
        exit(1);
    }

    /* generate a random, locally-administered and unicast MAC address */
    int rfd = open("/dev/urandom", O_RDONLY);

    if (rfd == -1)
        err(1, "Could not open /dev/urandom");

    uint8_t guest_mac[6];
    int ret;

    ret = read(rfd, guest_mac, sizeof(guest_mac));
    assert(ret == sizeof(guest_mac));
    close(rfd);
    guest_mac[0] &= 0xfe;
    guest_mac[0] |= 0x02;
    snprintf(netinfo.mac_str, sizeof(netinfo.mac_str),
            "%02x:%02x:%02x:%02x:%02x:%02x",
            guest_mac[0], guest_mac[1], guest_mac[2],
            guest_mac[3], guest_mac[4], guest_mac[5]);

    return 0;
}

static int get_fd(void)
{
    return netfd;
}

static char *usage(void)
{
    return "--net=TAP (host tap device for guest network interface)";
}

struct ukvm_module ukvm_net = {
    .get_fd = get_fd,
    .handle_exit = handle_exit,
    .handle_cmdarg = handle_cmdarg,
    .setup = setup,
    .usage = usage,
    .name = "net"
};
