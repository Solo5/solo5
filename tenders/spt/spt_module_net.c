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
 * spt_module_net.c: Network device module.
 */

#include <assert.h>
#include <err.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <seccomp.h>
#include <sys/epoll.h>

#include "../common/tap_attach.h"
#include "spt.h"

static bool module_in_use;

static int handle_cmdarg(char *cmdarg, struct mft *mft)
{
    enum {
        opt_net,
        opt_net_mac
    } which;

    if (strncmp("--net:", cmdarg, 6) == 0)
        which = opt_net;
    else if (strncmp("--net-mac:", cmdarg, 10) == 0)
        which = opt_net_mac;
    else
        return -1;

    char name[MFT_NAME_SIZE];
    char iface[20]; /* XXX should be IFNAMSIZ, needs extra header here */
    int rc;
    if (which == opt_net) {
        rc = sscanf(cmdarg,
                "--net:%" XSTR(MFT_NAME_MAX) "[A-Za-z0-9]="
                "%19s", name, iface);
        if (rc != 2)
            return -1;
        struct mft_entry *e = mft_get_by_name(mft, name, MFT_DEV_NET_BASIC,
                NULL);
        if (e == NULL) {
            warnx("Resource not declared in manifest: '%s'", name);
            return -1;
        }
        int fd = tap_attach(iface);
        if (fd < 0) {
            warnx("Could not attach interface: %s", iface);
            return -1;
        }

        /* e->u.net_basic.mac[] is set either by option or generated later by
         * setup().
         */
        e->u.net_basic.mtu = 1500; /* TODO */
        e->b.hostfd = fd;
        e->attached = true;
        module_in_use = true;
    }
    else if (which == opt_net_mac) {
        uint8_t mac[6];
        rc = sscanf(cmdarg,
                "--net-mac:%" XSTR(MFT_NAME_MAX) "[A-Za-z0-9]="
                "%02"SCNx8":%02"SCNx8":%02"SCNx8":"
                "%02"SCNx8":%02"SCNx8":%02"SCNx8,
                name,
                &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
        if (rc != 7)
            return -1;
        struct mft_entry *e = mft_get_by_name(mft, name, MFT_DEV_NET_BASIC,
                NULL);
        if (e == NULL) {
            warnx("Resource not declared in manifest: '%s'", name);
            return -1;
        }
        memcpy(e->u.net_basic.mac, mac, sizeof mac);
    }

    return 0;
}

static int setup(struct spt *spt, struct mft *mft)
{
    if (!module_in_use)
        return 0;

    for (unsigned i = 0; i != mft->entries; i++) {
        if (mft->e[i].type != MFT_DEV_NET_BASIC || !mft->e[i].attached)
            continue;
        char no_mac[6] = { 0 };
        if (memcmp(mft->e[i].u.net_basic.mac, no_mac, sizeof no_mac) == 0)
            tap_attach_genmac(mft->e[i].u.net_basic.mac);

        int rc;
        struct epoll_event ev;
        ev.events = EPOLLIN;
        /*
         * i is the manifest index, i.e. the solo5_handle_t and will be returned
         * by epoll() as part of any received event.
         */
        ev.data.u64 = i;
        rc = epoll_ctl(spt->epollfd, EPOLL_CTL_ADD, mft->e[i].b.hostfd, &ev);
        if (rc == -1)
            err(1, "epoll_ctl(EPOLL_CTL_ADD, hostfd=%d) failed",
                    mft->e[i].b.hostfd);

        rc = seccomp_rule_add(spt->sc_ctx, SCMP_ACT_ALLOW, SCMP_SYS(read), 1,
                SCMP_A0(SCMP_CMP_EQ, mft->e[i].b.hostfd));
        if (rc != 0)
            errx(1, "seccomp_rule_add(read, fd=%d) failed: %s",
                    mft->e[i].b.hostfd, strerror(-rc));
        rc = seccomp_rule_add(spt->sc_ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 1,
                SCMP_A0(SCMP_CMP_EQ, mft->e[i].b.hostfd));
        if (rc != 0)
            errx(1, "seccomp_rule_add(write, fd=%d) failed: %s",
                    mft->e[i].b.hostfd, strerror(-rc));
    }

    return 0;
}

static char *usage(void)
{
    return "--net:NAME=IFACE | @NN (attach tap at IFACE or at fd @NN as network NAME)\n"
        "  [ --net-mac:NAME=HWADDR ] (set HWADDR for network NAME)";
}

DECLARE_MODULE(net,
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage
)
