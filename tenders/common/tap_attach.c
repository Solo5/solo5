/*
 * Copyright (c) 2015-2019 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a sandboxed execution environment.
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
 * tap_attach.c: Common functions for attaching to TAP interfaces.
 */

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits.h>
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

#elif defined(__OpenBSD__)

#include <sys/socket.h>
#include <net/if.h>

#else /* !__linux__ && !__FreeBSD__ && !__OpenBSD__ */

#error Unsupported target

#endif

int tap_attach(const char *ifname)
{
    int fd;

    /*
     * Syntax @<number> indicates a pre-existing open fd, so just pass it
     * through if the supplied <number> is in range and O_NONBLOCK can be set.
     */
    if (ifname[0] == '@') {
        char *endp;
        long int maybe_fd = strtol(&ifname[1], &endp, 10);
        if (*endp != 0 /* Invalid character at (*endp)? */
            || endp == &ifname[1] /* Empty string? */)
            errno = EINVAL;
        else if (maybe_fd < 0 || maybe_fd > INT_MAX)
            errno = ERANGE;
        if (errno)
            return -1;

        fd = (int)maybe_fd;
        if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
            return -1;

        return fd;
    }
    else if (strlen(ifname) >= IFNAMSIZ) {
        errno = ENAMETOOLONG;
        return -1;
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
        if (strncmp(ifp->ifa_name, ifname, IFNAMSIZ) == 0) {
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
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

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

#elif defined(__OpenBSD__)

    if (!up) {
        errno = ENETDOWN;
        return -1;
    }

    char devname[strlen(ifname) + 6];

    snprintf(devname, sizeof devname, "/dev/%s", ifname);
    fd = open(devname, O_RDWR | O_NONBLOCK);
    if (fd == -1)
        return -1;
#endif

    return fd;
}

void tap_attach_genmac(uint8_t *mac)
{
    int rfd = open("/dev/urandom", O_RDONLY);

    if (rfd == -1)
        err(1, "Could not open /dev/urandom");

    int ret;

    ret = read(rfd, mac, 6);
    assert(ret == 6);
    close(rfd);
    mac[0] &= 0xfe;
    mac[0] |= 0x02;
}
