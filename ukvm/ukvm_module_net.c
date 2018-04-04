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

#define _BSD_SOURCE
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
#include <sys/mman.h>
#include <unistd.h>

#if defined(__linux__)

/*
 * Linux TAP device specific.
 */
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/kvm.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include "ukvm.h"
#include "ukvm_guest.h"
#include "ukvm_hv_kvm.h"
#include "ukvm_cpu_x86_64.h"
#include "shm_net.h"
#include "writer.h"
#define MAXEVENTS 5
static pthread_t tid;
struct muchannel *tx_channel;
struct muchannel *rx_channel;
struct muchannel_reader net_rdr;

#elif defined(__FreeBSD__)

#include <net/if.h>

#else /* !__linux__ && !__FreeBSD__ */

#error Unsupported target

#endif

#include "ukvm.h"

#define MAX_PACKETS_READ 100
static char *netiface;
static int netfd, solo5_rx_fd, solo5_tx_xon_fd, shm_rx_fd, shm_tx_fd, epoll_fd;
static struct ukvm_netinfo netinfo;
static int cmdline_mac = 0;
static int use_shm_stream = 0;
static int use_event_thread = 0;
static uint64_t rx_shm_size = 0x0;
static uint64_t tx_shm_size = 0x0;
struct timespec readtime;
struct timespec writetime;
struct timespec epochtime;

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

/* Solo5 is woken up when the shmstream is writable again */
void read_solo5_tx_xon_fd()
{
    uint64_t clear = 0;
    if (read(solo5_tx_xon_fd, &clear, 8) < 0) {}
}

/* Solo5 is woken up when there is data to be read from
 * shmstream */
void read_solo5_rx_fd()
{
    uint64_t clear = 0;

    /* Clears the notification */
    if (read(solo5_rx_fd, &clear, 8) < 0) {}
}

void* io_event_loop()
{
    struct net_msg pkt = { 0 };
    int ret, n, i, er;
    uint64_t clear = 0, wrote = 1;
    struct epoll_event event;
    struct epoll_event *events;
    uint64_t packets_read = 0;

    events = calloc(MAXEVENTS, sizeof event);

    while (1) {
        n = epoll_wait(epoll_fd, events, MAXEVENTS, -1);
        for (i = 0; i < n; i++) {
			if ((events[i].events & EPOLLERR) ||
				  (events[i].events & EPOLLHUP) ||
				  (!(events[i].events & EPOLLIN)))
			{
			  warnx("epoll error\n");
			  close(events[i].data.fd);
			  continue;
			} else if (netfd == events[i].data.fd) {
                packets_read = 0;
                while (packets_read < MAX_PACKETS_READ &&
                    ((ret = read(netfd, pkt.data, PACKET_SIZE)) > 0)) {
                    if (shm_net_write(tx_channel, pkt.data, ret) != SHM_NET_OK) {
                        /* Don't read from netfd. Instead, wait for tx_channel to
                         * be writable */
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, netfd, NULL);
                        break;
                    }
                    packets_read++;
                }
                if (packets_read) {
                    ret = write(solo5_rx_fd, &packets_read, 8);
                }
            } else if (shm_tx_fd == events[i].data.fd) {
                /* tx channel is writable again */
                warnx("tx shmstream is writable again");
                if (read(shm_tx_fd, &clear, 8) < 0) {}

                /* Start reading from netfd */
                event.data.fd = netfd;
                event.events = EPOLLIN;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, netfd, &event);
            } else if (shm_rx_fd == events[i].data.fd) {
                /* Read data from shmstream and write to tap interface */
                do {
                    ret = shm_net_read(rx_channel, &net_rdr,
                        pkt.data, PACKET_SIZE, (size_t *)&pkt.length);
                    if ((ret == SHM_NET_OK) || (ret == SHM_NET_XON)) {
                        if (ret == SHM_NET_XON) {
                            er = write(solo5_tx_xon_fd, &wrote, 8);
                        }
                        er = write(netfd, pkt.data, pkt.length);
                        assert(er == pkt.length);
                    } else if (ret == SHM_NET_AGAIN) {
                        if (read(shm_rx_fd, &clear, 8) < 0) {}
                        break;
                    } else if (ret == SHM_NET_EPOCH_CHANGED) {
                        /* Don't clear the eventfd */
                        break;
                    } else {
                        warnx("Failed to read from shmstream");
                        assert(0);
                    }
                } while (1);
            }
		}
    }
}

void* io_thread()
{
    struct net_msg pkt;
    int ret, er, tap_no_data = 0, shm_no_data = 0;
    uint64_t packets_read = 0;

    while (1) {
        /* Read packets from tap interface and write to shmstream */
        while (packets_read < MAX_PACKETS_READ &&
            ((ret = read(netfd, pkt.data, PACKET_SIZE)) > 0)) {
            packets_read++;
            if (shm_net_write(tx_channel, pkt.data, ret) != SHM_NET_OK) {
                ret = 0;
                break;
            }
        }

        if ((ret == 0) ||
            (ret == -1 && errno == EAGAIN)) {
            /* NO data */
            tap_no_data = 1;
        } else if (ret < 0) {
            /* error */
            assert(0);
        }
        if (packets_read) {
            /* Notify the reader of shmstream */
            ret = write(solo5_rx_fd, &packets_read, 8);
            packets_read = 0;
        }

        /* Read from shmstream and write to tap interface */
        do {
            ret = shm_net_read(rx_channel, &net_rdr,
                pkt.data, PACKET_SIZE, (size_t *)&pkt.length);
            if (ret == SHM_NET_OK || ret == SHM_NET_XON) {
                er = write(netfd, pkt.data, pkt.length);
                assert(er == pkt.length);
            }
        } while(++packets_read < MAX_PACKETS_READ &&
            (ret == SHM_NET_OK || ret == SHM_NET_XON));

        if (ret == SHM_NET_AGAIN) {
            shm_no_data = 1;
        } else if (ret == SHM_NET_EINVAL) {
            warnx("Invalid error when read from shmstream");
            assert(0);
        }

        if (tap_no_data && shm_no_data) {
            /* Sleep for a millisec */
            usleep(1);
        }
        packets_read = 0;
        tap_no_data = 0;
        shm_no_data = 0;
    }
}

static void hypercall_net_shm_info(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    struct ukvm_net_shm_info *info =
            UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_net_shm_info));

    /* Start the thread */
    if (info->completed) {
        muen_channel_init_reader(&net_rdr, MUENNET_PROTO);
        if (use_event_thread) {
            pthread_create(&tid, NULL, &io_event_loop, NULL);
        } else {
            pthread_create(&tid, NULL, &io_thread, NULL);
        }
    } else if (use_shm_stream) {
        info->tx_channel_addr = hv->mem_size;
        info->tx_channel_addr_size = rx_shm_size;

        info->rx_channel_addr = hv->mem_size + rx_shm_size;
        info->rx_channel_addr_size = tx_shm_size;
        if (use_event_thread) {
            info->shm_event_enabled = true;
        } else {
            info->shm_poll_enabled = true;
        }
    }
}

static void hypercall_netinfo(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    struct ukvm_netinfo *info =
        UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_netinfo));

    memcpy(info->mac_address, netinfo.mac_address, sizeof(netinfo.mac_address));
}

static void hypercall_netwrite(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    struct ukvm_netwrite *wr =
        UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_netwrite));
    int ret;

    ret = write(netfd, UKVM_CHECKED_GPA_P(hv, wr->data, wr->len), wr->len);
    assert(wr->len == ret);
    wr->ret = 0;
}

static void hypercall_netread(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    struct ukvm_netread *rd =
        UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_netread));
    int ret;

    ret = read(netfd, UKVM_CHECKED_GPA_P(hv, rd->data, rd->len), rd->len);
    if ((ret == 0) ||
        (ret == -1 && errno == EAGAIN)) {
        rd->ret = -1;
        return;
    }
    assert(ret > 0);
    rd->len = ret;
    rd->ret = 0;
}

static void hypercall_netxon(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    uint64_t xon = 1;
    if (write(shm_tx_fd, &xon, 8)) {}
}

static void hypercall_netnotify(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    uint64_t read_data = 1;
    if (write(shm_rx_fd, &read_data, 8)) {}
}

static int handle_cmdarg(char *cmdarg)
{
    if (!strncmp("--net=", cmdarg, 6)) {
        netiface = cmdarg + 6;
        return 0;
    } else if (!strncmp("--net-mac=", cmdarg, 10)) {
        const char *macptr = cmdarg + 10;
        uint8_t mac[6];
        if (sscanf(macptr,
                   "%02"SCNx8":%02"SCNx8":%02"SCNx8":"
                   "%02"SCNx8":%02"SCNx8":%02"SCNx8,
                   &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
            warnx("Malformed mac address: %s", macptr);
            return -1;
        }
        memcpy(netinfo.mac_address, mac, sizeof netinfo.mac_address);
        cmdline_mac = 1;
        return 0;
    } else if (!strncmp("--shm=poll", cmdarg, 10)) {
        use_shm_stream = 1;
        return 0;
    } else if (!strncmp("--shm=event", cmdarg, 11)) {
        use_shm_stream = 1;
        use_event_thread = 1;
        return 0;
    } else {
        return -1;
    }
}

static int configure_epoll()
{
    struct epoll_event event;

    if ((epoll_fd = epoll_create1(0)) < 0) {
        warnx("Failed to create epoll fd");
        return -1;
    }

    event.data.fd = netfd;
    event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, netfd, &event) < 0) {
        warnx("Failed to set up fd at epoll_ctl");
        return -1;
    }

    if ((shm_rx_fd = eventfd(0, EFD_NONBLOCK)) < 0) {
        warnx("Failed to create eventfd");
        return -1;
    }

    event.data.fd = shm_rx_fd;
    event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, shm_rx_fd, &event) < 0) {
        warnx("Failed to set up fd at epoll_ctl");
        return -1;
    }

    if ((shm_tx_fd = eventfd(0, EFD_NONBLOCK)) < 0) {
        warnx("Failed to create eventfd");
        return -1;
    }

    event.data.fd = shm_tx_fd;
    event.events = EPOLLIN;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, shm_tx_fd, &event);

    solo5_tx_xon_fd = eventfd(0, EFD_NONBLOCK);
    if (solo5_tx_xon_fd < 0) {
        warnx("Failed to create eventfd");
        return -1;
    }

    return 0;
}

static int configure_shmstream(struct ukvm_hv *hv)
{
    uint64_t offset = 0x0;
    uint8_t *shm_mem;
    uint64_t total_pagesize = 0x0;
    //size_t ring_buf_size = sizeof(struct net_msg);
    rx_shm_size = 0x250000;//1000 * ring_buf_size;
    tx_shm_size = 0x250000;//1000 * ring_buf_size;
    int ret;

    /* Set up eventfd */
    solo5_rx_fd = eventfd(0, EFD_NONBLOCK);
    if (solo5_rx_fd < 0) {
        err(1, "Failed to create eventfd");
        goto err;
    }

    if (use_event_thread) {
        /* Set up epoll */
        if ((configure_epoll()) < 0) {
            err(1, "Failed to configure epoll");
            goto err;
        }
    }

    /* Find out additional memory required to configure shm */
    uint64_t total_shm_buf_size = rx_shm_size + tx_shm_size;

    shm_mem = mmap(NULL, total_shm_buf_size, PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shm_mem == MAP_FAILED) {
        err(1, "Failed allocate memory for shmstream");
        goto err;
    }

    do {
        total_pagesize += X86_GUEST_PAGE_SIZE;
    } while(total_pagesize < total_shm_buf_size);

    if ((hv->mem_size + total_shm_buf_size) > (X86_GUEST_PAGE_SIZE * 512)) {
        err(1, "guest memory size exceeds the max size %u bytes", X86_GUEST_PAGE_SIZE * 512);
        goto err;
    }

    /* RX ring buffer */
    struct kvm_userspace_memory_region rxring_region = {
        .slot = UKVM_SHMSTREAM_RXRING_BUF_REGION,
        .guest_phys_addr = hv->mem_size + offset,
        .memory_size = (uint64_t)(rx_shm_size),
        .userspace_addr = (uint64_t)(shm_mem + offset),
    };
    ret = ioctl(hv->b->vmfd, KVM_SET_USER_MEMORY_REGION, &rxring_region);
    if (ret == -1) {
        err(1, "KVM: ioctl (SET_USER_MEMORY_REGION) failed: shmstream RX ring buffer");
        goto err;
    }
    rx_channel = (struct muchannel *)(shm_mem + offset);
    offset += rxring_region.memory_size;

    /* TX ring buffer */
    struct kvm_userspace_memory_region txring_region = {
        .slot = UKVM_SHMSTREAM_TXRING_BUF_REGION,
        .guest_phys_addr = hv->mem_size + offset,
        .memory_size = (uint64_t)(tx_shm_size),
        .userspace_addr = (uint64_t)(shm_mem + offset),
    };
    ret = ioctl(hv->b->vmfd, KVM_SET_USER_MEMORY_REGION, &txring_region);
    if (ret == -1) {
        err(1, "KVM: ioctl (SET_USER_MEMORY_REGION) failed: shmstream TX ring buffer");
        goto err;
    }

    /* Init tx ring as a writer */
    tx_channel = (struct muchannel *)(shm_mem + offset);
    /* TODO: Use monotonic epoch in kernel as well*/
    clock_gettime(CLOCK_MONOTONIC, &epochtime);
    muen_channel_init_writer(tx_channel, MUENNET_PROTO, sizeof(struct net_msg),
            tx_shm_size, epochtime.tv_nsec);
    offset += txring_region.memory_size;

    printf("offset = 0x%"PRIx64", total_pagesize = 0x%"PRIx64"\n", offset, total_pagesize);
    ukvm_x86_add_pagetables(hv->mem, hv->mem_size, total_pagesize);
    return 0;

err:
    return -1;
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
        memcpy(netinfo.mac_address, guest_mac, sizeof netinfo.mac_address);
    }

    if (use_shm_stream) {
        int flags = fcntl(netfd, F_GETFL, 0);
        fcntl(netfd, F_SETFL, flags | O_NONBLOCK);

        if (configure_shmstream(hv)) {
            err(1, "Failed to configure shmstream");
            exit(1);
        }
    }

    assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_NETINFO,
                hypercall_netinfo) == 0);
    assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_NETWRITE,
                hypercall_netwrite) == 0);
    assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_NETREAD,
                hypercall_netread) == 0);
    assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_NET_SHMINFO,
                hypercall_net_shm_info) == 0);
    if (use_event_thread) {
        assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_NETXON,
                    hypercall_netxon) == 0);
        assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_NETNOTIFY,
                    hypercall_netnotify) == 0);
    }

    /* If shmstream is used, register eventfd, else
     * use tapfd */
    if (use_shm_stream) {
        assert(ukvm_core_register_pollfd(solo5_rx_fd, read_solo5_rx_fd) == 0);
        if (use_event_thread) {
            assert(ukvm_core_register_pollfd(solo5_tx_xon_fd, read_solo5_tx_xon_fd) == 0);
        }
    } else {
        assert(ukvm_core_register_pollfd(netfd, NULL) == 0);
    }

    return 0;
}

static char *usage(void)
{
    return "--net=TAP (host tap device for guest network interface or @NN tap fd)\n"
        "    [ --net-mac=HWADDR ] (guest MAC address)";
}

static void cleanup(struct ukvm_hv *hv)
{
    if (use_shm_stream) {
        if (pthread_cancel(tid) == 0) {
            pthread_join(tid, NULL);
        }
    }
}

struct ukvm_module ukvm_module_net = {
    .name = "net",
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage,
    .cleanup = cleanup
};
