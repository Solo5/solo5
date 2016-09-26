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
 * Create or reuse a TUN or TAP device named 'dev'.
 *
 * Copied from kernel docs: Documentation/networking/tuntap.txt
 */
static int tun_alloc(char *dev, int flags)
{
    struct ifreq ifr;
    int fd, err;
    char *clonedev = "/dev/net/tun";

    /* Arguments taken by the function:
     *
     * char *dev: the name of an interface (or '\0'). MUST have enough
     *   space to hold the interface name if '\0' is passed
     * int flags: interface flags (eg, IFF_TUN etc.)
     */

    /* open the clone device */
    fd = open(clonedev, O_RDWR | O_NONBLOCK);
    if (fd < 0)
        return fd;

    /* preparation of the struct ifr, of type "struct ifreq" */
    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = flags;	/* IFF_TUN or IFF_TAP, plus maybe IFF_NO_PI */

    if (*dev) {
        /* if a device name was specified, put it in the structure; otherwise,
         * the kernel will try to allocate the "next" device of the
         * specified type
         */
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }

    /* try to create the device */
    err = ioctl(fd, TUNSETIFF, (void *) &ifr);
    if (err < 0) {
        close(fd);
        return err;
    }

    /* if the operation was successful, write back the name of the
     * interface to the variable "dev", so the caller can know
     * it. Note that the caller MUST reserve space in *dev (see calling
     * code below)
     */
    strcpy(dev, ifr.ifr_name);

    /* this is the special file descriptor that the caller will use to talk
     * with the virtual interface
     */
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
    char tun_name[IFNAMSIZ];

    if (netiface == NULL)
        return -1;

    /* set up virtual network */
    strcpy(tun_name, netiface);
    netfd = tun_alloc(tun_name, IFF_TAP | IFF_NO_PI);	/* TAP interface */
    if (netfd < 0) {
        perror("Allocating interface");
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

    printf("Providing network: %s, guest address %s\n", tun_name,
            netinfo.mac_str);

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
    .usage = usage
};
