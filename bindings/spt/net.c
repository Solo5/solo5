/*
 * Copyright (c) 2015-2018 Contributors as noted in the AUTHORS file
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

#include "bindings.h"

static int netfd = -1;
static uint8_t mac_address[6];

void net_init(struct spt_boot_info *bi)
{
    if (bi->neti.present) {
        netfd = bi->neti.hostfd;
        memcpy(mac_address, bi->neti.mac_address, sizeof mac_address);
    }
}

void solo5_net_info(struct solo5_net_info *info)
{
    assert(netfd >= 0);

    memcpy(info->mac_address, mac_address, sizeof info->mac_address);
    info->mtu = 1500;
}

solo5_result_t solo5_net_read(uint8_t *buf, size_t size, size_t *read_size)
{
    assert(netfd >= 0);
    
    int nbytes = sys_read(netfd, (char *)buf, size);
    if (nbytes < 0) {
        if (nbytes == SYS_EAGAIN)
            return SOLO5_R_AGAIN;
        else
            return SOLO5_R_EUNSPEC;
    }

    *read_size = (size_t)nbytes;
    return SOLO5_R_OK;
}

solo5_result_t solo5_net_write(const uint8_t *buf, size_t size)
{
    assert(netfd >= 0);

    int nbytes = sys_write(netfd, (const char *)buf, size);

    return (nbytes == (int)size) ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
}

bool solo5_yield(solo5_time_t deadline)
{
    struct sys_timespec ts;
    int rc;
    struct sys_pollfd fds[1];
    int nfds = 0;
    uint64_t now, timeout_nsecs;

    now = solo5_clock_monotonic();
    if (deadline <= now)
        timeout_nsecs = 0;
    else
        timeout_nsecs = deadline - now;

    ts.tv_sec = timeout_nsecs / NSEC_PER_SEC;
    ts.tv_nsec = timeout_nsecs % NSEC_PER_SEC;

    if (netfd >= 0) {
        fds[nfds].fd = netfd;
        fds[nfds].events = SYS_POLLIN;
        nfds++;
    }

    do {
        rc = sys_ppoll(fds, nfds, &ts);
    } while (rc == SYS_EINTR);
    assert(rc >= 0);
    
    return rc;
}
