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

#if HVT_FREEBSD_ENABLE_CAPSICUM
#include <sys/capsicum.h>
#endif

#include "../common/tap_attach.h"
#include "hvt.h"
#include "solo5.h"

static bool module_in_use;
static struct mft *host_mft;

static void hypercall_net_write(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_hc_net_write *wr =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_hc_net_write));
    struct mft_entry *e = mft_get_by_index(host_mft, wr->handle,
            MFT_DEV_NET_BASIC);
    if (e == NULL) {
        wr->ret = SOLO5_R_EINVAL;
        return;
    }

    int ret;

    ret = write(e->b.hostfd, HVT_CHECKED_GPA_P(hvt, wr->data, wr->len),
            wr->len);
    assert(wr->len == ret);
    wr->ret = SOLO5_R_OK;
}

static void hypercall_net_read(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_hc_net_read *rd =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof (struct hvt_hc_net_read));
    struct mft_entry *e = mft_get_by_index(host_mft, rd->handle,
            MFT_DEV_NET_BASIC);
    if (e == NULL) {
        rd->ret = SOLO5_R_EINVAL;
        return;
    }

    int ret;

    ret = read(e->b.hostfd, HVT_CHECKED_GPA_P(hvt, rd->data, rd->len), rd->len);
    if ((ret == 0) ||
        (ret == -1 && errno == EAGAIN)) {
        rd->ret = SOLO5_R_AGAIN;
        return;
    }
    assert(ret > 0);
    rd->len = ret;
    rd->ret = SOLO5_R_OK;
}

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

static int setup(struct hvt *hvt, struct mft *mft)
{
    if (!module_in_use)
        return 0;

    host_mft = mft;
    assert(hvt_core_register_hypercall(HVT_HYPERCALL_NET_WRITE,
                hypercall_net_write) == 0);
    assert(hvt_core_register_hypercall(HVT_HYPERCALL_NET_READ,
                hypercall_net_read) == 0);

    for (unsigned i = 0; i != mft->entries; i++) {
        if (mft->e[i].type != MFT_DEV_NET_BASIC || !mft->e[i].attached)
            continue;
        char no_mac[6] = { 0 };
        if (memcmp(mft->e[i].u.net_basic.mac, no_mac, sizeof no_mac) == 0)
            tap_attach_genmac(mft->e[i].u.net_basic.mac);
        assert(hvt_core_register_pollfd(mft->e[i].b.hostfd, i) == 0);
    }

#if HVT_FREEBSD_ENABLE_CAPSICUM
    cap_rights_t rights;
    cap_rights_init(&rights, CAP_EVENT, CAP_WRITE, CAP_READ);
    if (cap_rights_limit(netfd, &rights) == -1)
        err(1, "cap_rights_limit() failed");
#endif

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
