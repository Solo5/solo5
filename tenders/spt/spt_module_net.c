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

#include "../common/tap_attach.h"
#include "spt.h"

static char *netiface;
static int netfd = -1;
static uint8_t guest_mac[6];
static int cmdline_mac = 0;

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
        memcpy(guest_mac, mac, sizeof guest_mac);
        cmdline_mac = 1;
        return 0;
    } else {
        return -1;
    }
}

static int setup(struct spt *spt)
{
    if (netiface == NULL)
        return 0; /* not set up == not present */

    netfd = tap_attach(netiface);
    if (netfd < 0)
        err(1, "Could not attach interface: %s", netiface);

    if (!cmdline_mac)
	tap_attach_genmac(guest_mac);

    spt->bi->neti.present = 1;
    memcpy(spt->bi->neti.mac_address, guest_mac,
            sizeof spt->bi->neti.mac_address);
    spt->bi->neti.hostfd = netfd;

    int rc = -1;

    rc = seccomp_rule_add(spt->sc_ctx, SCMP_ACT_ALLOW, SCMP_SYS(read), 1,
            SCMP_A0(SCMP_CMP_EQ, netfd));
    if (rc != 0)
        errx(1, "seccomp_rule_add(read, fd=%d) failed: %s", netfd,
                strerror(-rc));
    rc = seccomp_rule_add(spt->sc_ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 1,
            SCMP_A0(SCMP_CMP_EQ, netfd));
    if (rc != 0)
        errx(1, "seccomp_rule_add(write, fd=%d) failed: %s", netfd,
                strerror(-rc));

    return 0;
}

static char *usage(void)
{
    return "--net=TAP (host tap device for guest network interface or @NN tap fd)\n"
        "    [ --net-mac=HWADDR ] (guest MAC address)";
}

struct spt_module spt_module_net = {
    .name = "net",
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage
};
