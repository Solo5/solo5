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
 * hvt_module_net.c: Network device module.
 */

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../common/tap_attach.h"
#include "hvt.h"

static char *netiface;
static int netfd;
static struct hvt_netinfo netinfo;
static int cmdline_mac = 0;

static void hypercall_netinfo(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_netinfo *info =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_netinfo));

    memcpy(info->mac_address, netinfo.mac_address, sizeof(netinfo.mac_address));
}

static void hypercall_netwrite(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_netwrite *wr =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_netwrite));
    int ret;

    ret = write(netfd, HVT_CHECKED_GPA_P(hvt, wr->data, wr->len), wr->len);
    assert(wr->len == ret);
    wr->ret = 0;
}

static void hypercall_netread(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_netread *rd =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_netread));
    int ret;

    ret = read(netfd, HVT_CHECKED_GPA_P(hvt, rd->data, rd->len), rd->len);
    if ((ret == 0) ||
        (ret == -1 && errno == EAGAIN)) {
        rd->ret = -1;
        return;
    }
    assert(ret > 0);
    rd->len = ret;
    rd->ret = 0;
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
    } else {
        return -1;
    }
}

static int setup(struct hvt *hvt)
{
    if (netiface == NULL)
        return 0; /* Not present */

    netfd = tap_attach(netiface);
    if (netfd < 0)
        err(1, "Could not attach interface: %s", netiface);

    if (!cmdline_mac)
	tap_attach_genmac(netinfo.mac_address);

    assert(hvt_core_register_hypercall(HVT_HYPERCALL_NETINFO,
                hypercall_netinfo) == 0);
    assert(hvt_core_register_hypercall(HVT_HYPERCALL_NETWRITE,
                hypercall_netwrite) == 0);
    assert(hvt_core_register_hypercall(HVT_HYPERCALL_NETREAD,
                hypercall_netread) == 0);
    assert(hvt_core_register_pollfd(netfd) == 0);

    return 0;
}

static char *usage(void)
{
    return "--net=TAP (host tap device for guest network interface or @NN tap fd)\n"
        "    [ --net-mac=HWADDR ] (guest MAC address)";
}

BEGIN_REGISTER_MODULE(net) {
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage
}
END_REGISTER_MODULE
