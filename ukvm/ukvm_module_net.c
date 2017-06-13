/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of ukvm, a unikernel monitor.
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * ukvm_module_net.c: Network device module.
 */

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#if defined(__linux__)

/*
 * Linux TAP device specific.
 */
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>

#elif defined(__FreeBSD__)

#include <net/if.h>

#else /* !__linux__ && !__FreeBSD__ */

#error Unsupported target

#endif

#include "ukvm.h"

static char *netiface;
static int netfd;
static struct ukvm_netinfo netinfo;
static int cmdline_mac = 0;

#define SOLO5_NET_BUF_SIZE 1514

/*
 * Attach to an existing TAP interface named 'ifname'.
 *
 * Returns -1 and an appropriate errno on failure (ENOENT if the interface does
 * not exist), and the tap device file descriptor on success.
 */
static int tap_attach(const char *ifname)
{
    int fd;

    /*
     * Syntax @<number> indicates a pre-existing open fd, so just pass it
     * through without any checks.
     */
    if (ifname[0] == '@') {
        fd = atoi(&ifname[1]);

        if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
            return -1;

        return fd;
    }

    /*
     * Verify that the interface exists and is up and running. If we don't do
     * this then we get "create on open" behaviour on most systems which is not
     * what we want.
     */
    struct ifaddrs *ifa, *ifp;
    int found = 0;
    int up = 0;

    if (getifaddrs(&ifa) == -1)
        return -1;
    ifp = ifa;
    while (ifp) {
        if (strcmp(ifp->ifa_name, ifname) == 0) {
            found = 1;
            up = ifp->ifa_flags & (IFF_UP | IFF_RUNNING);
            break;
        }
        ifp = ifp->ifa_next;
    }
    freeifaddrs(ifa);
    if (!found) {
        errno = ENOENT;
        return -1;
    }

#if defined(__linux__)

    if (!up) {
        errno = ENETDOWN;
        return -1;
    }

    int err;
    struct ifreq ifr;

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
    if (strlen(ifname) > IFNAMSIZ) {
        errno = EINVAL;
        return -1;
    }
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

    /*
     * Attach to the tap device; we have already verified that it exists, but
     * see below.
     */
    if (ioctl(fd, TUNSETIFF, (void *)&ifr) == -1) {
        err = errno;
        close(fd);
        errno = err;
        return -1;
    }
    /*
     * If we got back a different device than the one requested, e.g. because
     * the caller mistakenly passed in '%d' (yes, that's really in the Linux
     * API) then fail.
     */
    if (strncmp(ifr.ifr_name, ifname, IFNAMSIZ) != 0) {
        close(fd);
        errno = EINVAL;
        return -1;
    }

#elif defined(__FreeBSD__)

    char devname[strlen(ifname) + 6];

    snprintf(devname, sizeof devname, "/dev/%s", ifname);
    fd = open(devname, O_RDWR | O_NONBLOCK);
    if (fd == -1)
        return -1;

#endif

    return fd;
}

static void hypercall_netinfo(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    struct ukvm_netinfo *info =
        UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_netinfo));

    memcpy(info->mac_str, netinfo.mac_str, sizeof(netinfo.mac_str));
}

static void hypercall_netwrite(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    struct ukvm_netwrite *wr =
        UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_netwrite));
    ssize_t nbytes;

    if (wr->len > SOLO5_NET_BUF_SIZE) {
        wr->ret = -1;
        return;
    }

    nbytes = write(netfd, UKVM_CHECKED_GPA_P(hv, wr->data, wr->len), wr->len);
    if (nbytes != wr->len)
        wr->ret = -1;
    else
        wr->ret = 0;
}

static void hypercall_netread(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    struct ukvm_netread *rd =
        UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_netread));
    ssize_t nbytes;

    if (rd->len < SOLO5_NET_BUF_SIZE) {
        rd->ret = -1;
        return;
    }

    nbytes = read(netfd, UKVM_CHECKED_GPA_P(hv, rd->data, rd->len), rd->len);
    if ((nbytes == 0) ||
        (nbytes == -1 && errno == EAGAIN)) {
        rd->ret = 0;
        rd->len = 0;
        return;
    }
    if (nbytes == -1)
        rd->ret = -1;
    else {
        rd->ret = 0;
        rd->len = nbytes;
    }
}

static int handle_cmdarg(char *cmdarg)
{
    if (!strncmp("--net=", cmdarg, 6)) {
        netiface = cmdarg + 6;
        return 0;
    } else if (!strncmp("--net-mac=", cmdarg, 6)) {
        const char *macptr = cmdarg + 10;
        uint8_t mac[6];
        if (sscanf(macptr,
                   "%02"SCNx8":%02"SCNx8":%02"SCNx8":"
                   "%02"SCNx8":%02"SCNx8":%02"SCNx8,
                   &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
            warnx("Malformed mac address: %s", macptr);
            return -1;
        }
        snprintf(netinfo.mac_str, sizeof(netinfo.mac_str), "%s", macptr);
        cmdline_mac = 1;
        return 0;
    } else {
        return -1;
    }
}

static int setup(struct ukvm_hv *hv)
{
    if (netiface == NULL)
        return -1;

    /* attach to requested tap interface */
    netfd = tap_attach(netiface);
    if (netfd < 0) {
        err(1, "Could not attach interface: %s", netiface);
        exit(1);
    }

    if (!cmdline_mac) {
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
    }

    assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_NETINFO,
                hypercall_netinfo) == 0);
    assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_NETWRITE,
                hypercall_netwrite) == 0);
    assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_NETREAD,
                hypercall_netread) == 0);
    assert(ukvm_core_register_pollfd(netfd) == 0);

    return 0;
}

static char *usage(void)
{
    return "--net=TAP (host tap device for guest network interface or @NN tap fd)\n"
        "    [ --net-mac=HWADDR ] (guest MAC address)";
}

struct ukvm_module ukvm_module_net = {
    .name = "net",
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage
};
