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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if HVT_FREEBSD_ENABLE_CAPSICUM
#include <sys/capsicum.h>
#endif

#include "../common/tap_attach.h"
#include "hvt.h"
#include "solo5.h"

#include <pthread.h>
#include "hvt_ring.h"

#if defined(__linux__)
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <linux/kvm.h>
#include "hvt_kvm.h"
#elif defined(__FreeBSD__)
#include "hvt_freebsd.h"
#elif defined(__OpenBSD__)
#include "hvt_openbsd.h"
#endif

static bool module_in_use;
static struct mft *host_mft;
static volatile int io_thread_stop;
static hvt_gpa_t reserved_ring_gpa;

size_t hvt_net_mem_overhead(struct mft *mft)
{
    for (unsigned i = 0; i != mft->entries; i++) {
        if (mft->e[i].type == MFT_DEV_NET_BASIC && mft->e[i].attached)
            return sizeof(struct hvt_ring);
    }
    return 0;
}

void hvt_net_reserve_ring(struct hvt *hvt, struct mft *mft)
{
    /* A ring is allocated only if there is at least one network interface
     * listed in the manifest. The ring is then used by all interfaces. */
    for (unsigned i = 0; i != mft->entries; i++) {
        if (mft->e[i].type == MFT_DEV_NET_BASIC && mft->e[i].attached) {
            size_t ring_size = sizeof(struct hvt_ring);
            hvt_gpa_t gpa_ring = (hvt->guest_mem_size - ring_size) & ~0xFFFULL;

            if (gpa_ring < 0x200000) {
                warnx("Not enough guest memory for ring allocation");
                return;
            }

            memset(hvt->mem + gpa_ring, 0, ring_size);
            reserved_ring_gpa = gpa_ring;
            hvt->guest_mem_size = gpa_ring;
            return;
        }
    }
}

static void hypercall_net_write(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_hc_net_write *wr =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof(struct hvt_hc_net_write));
    struct mft_entry *e =
        mft_get_by_index(host_mft, wr->handle, MFT_DEV_NET_BASIC);
    if (e == NULL) {
        wr->ret = SOLO5_R_EINVAL;
        return;
    }

    ssize_t ret;

    ret =
        write(e->b.hostfd, HVT_CHECKED_GPA_P(hvt, wr->data, wr->len), wr->len);
    if (ret == -1) {
        fprintf(stderr, "Fatal error when writing: %s\n", strerror(errno));
        exit(1);
    } else if ((size_t)ret != wr->len) {
        fprintf(stderr, "Fatal error: wrote only %ld out of %ld bytes\n", ret,
                wr->len);
        exit(1);
    }
    wr->ret = SOLO5_R_OK;
}

static void hypercall_net_read(struct hvt *hvt, hvt_gpa_t gpa)
{
    struct hvt_hc_net_read *rd =
        HVT_CHECKED_GPA_P(hvt, gpa, sizeof(struct hvt_hc_net_read));
    struct mft_entry *e =
        mft_get_by_index(host_mft, rd->handle, MFT_DEV_NET_BASIC);
    if (e == NULL) {
        rd->ret = SOLO5_R_EINVAL;
        return;
    }

    ssize_t ret;

    ret = read(e->b.hostfd, HVT_CHECKED_GPA_P(hvt, rd->data, rd->len), rd->len);
    if ((ret == 0) || (ret == -1 && errno == EAGAIN)) {
        rd->ret = SOLO5_R_AGAIN;
        return;
    }
    if (ret <= 0) {
        if (ret == -1)
            fprintf(stderr, "Fatal error when reading: %s\n", strerror(errno));
        else
            fprintf(stderr, "Fatal error: read returned %ld\n", ret);
        exit(1);
    }
    rd->len = ret;
    rd->ret = SOLO5_R_OK;
}

struct io_thread_arg {
    struct hvt *hvt;
    struct hvt_ring *ring;
    int notify_fd;
    volatile int ready; /* set by the I/O thread once fully initialized */
};

/* Process a NET_READ submission entry. Reads directly from the TAP fd into
 * guest memory and posts a commit.
 */
static inline void process_read_entry(struct hvt *hvt, struct hvt_ring *ring,
                                      struct hvt_ring_entry *ent)
{
    uint32_t commit_idx = ring->com_tail & HVT_RING_MASK;
    struct hvt_ring_commit *commit = &ring->commits[commit_idx];

    commit->id = ent->id;

    struct mft_entry *e =
        mft_get_by_index(host_mft, ent->handle, MFT_DEV_NET_BASIC);

    if (e == NULL) {
        commit->ret = SOLO5_R_EINVAL;
        commit->len = 0;
    } else {
        void *data = HVT_CHECKED_GPA_P(hvt, ent->data, ent->len);
        ssize_t nr = read(e->b.hostfd, data, ent->len);

        if (nr == 0 || (nr == -1 && errno == EAGAIN)) {
            commit->ret = SOLO5_R_AGAIN;
            commit->len = 0;
        } else if (nr > 0) {
            commit->ret = SOLO5_R_OK;
            commit->len = nr;
        } else {
            commit->ret = SOLO5_R_EINVAL;
            commit->len = 0;
        }
    }

    hvt_wmb();
    ring->com_tail++;
    ring->ent_head++;
}

/*
 * Process all pending ring commits with batched ent_head updates for
 * consecutive writes. For N consecutive NET_WRITE entries, only a single
 * hvt_wmb() + ent_head update is issued instead of N.
 */
static inline void process_ring_commits(struct hvt *hvt, struct hvt_ring *ring)
{
    /*
     * Snapshot ent_tail after the caller's hvt_rmb(). On aarch64 (weakly
     * ordered), re-reading the volatile ent_tail inside the loop would let
     * the CPU observe a new tail value without a barrier, potentially
     * loading stale entry data for entries the guest just submitted.
     * Snapshotting avoids this: every entry up to tail_snap is guaranteed
     * visible by the preceding hvt_rmb().
     *
     * On x86 (TSO) this is harmless, loads are never reordered with loads.
     */
    uint32_t tail_snap = ring->ent_tail;

    while (ring->ent_head != tail_snap) {
        uint32_t batch_start = ring->ent_head;
        uint32_t processed = 0;

        /* Batch consecutive NET_WRITE entries */
        while (ring->ent_head + processed != tail_snap) {
            uint32_t idx = (batch_start + processed) & HVT_RING_MASK;
            struct hvt_ring_entry *ent = &ring->entries[idx];

            if (ent->operation == HVT_RING_NET_WRITE) {
                struct mft_entry *e =
                    mft_get_by_index(host_mft, ent->handle, MFT_DEV_NET_BASIC);

                if (e != NULL) {
                    void *data = HVT_CHECKED_GPA_P(hvt, ent->data, ent->len);
                    ssize_t ret = write(e->b.hostfd, data, ent->len);
                    if (ret == -1)
                        err(1, "Fatal write error on net device");
                    if ((size_t)ret != ent->len)
                        errx(1,
                             "Fatal write error: wrote only %zd"
                             " out of %u bytes",
                             ret, ent->len);
                }
                processed++;
            } else if (ent->operation == HVT_RING_NET_READ) {
                /*
                 * Flush the batch of writes before processing the read, which
                 * requires an immediate completion.
                 */
                if (processed > 0) {
                    hvt_wmb();
                    ring->ent_head += processed;
                    processed = 0;
                }
                process_read_entry(hvt, ring, ent);
                break; /* re-evaluate the outer loop */
            } else {
                /* Unknown operation: include in the batch to skip */
                processed++;
            }
        }

        if (processed > 0) {
            hvt_wmb();
            ring->ent_head += processed;
        }
    }
}

static void *io_thread_net_fn(void *arg)
{
    struct io_thread_arg *ta = arg;
    struct hvt *hvt = ta->hvt;
    struct hvt_ring *ring = ta->ring;
    int nfd = ta->notify_fd;

    /*
     * Signal the main thread that this thread is fully initialized.
     * On OpenBSD, hvt_drop_privileges() calls pledge("stdio vmm") after
     * setup_modules() returns. We must ensure that all pthread runtime
     * initialization (TLS, stack setup, etc.) has completed before pledge
     * restricts the available syscalls.
     */
    ta->ready = 1;

    while (!io_thread_stop) {
        /* Adaptive polling: spin-poll the ring for new submissions before
         * falling back to blocking on the notification fd. This eliminates the
         * read() syscall overhead during sustained traffic bursts.
         */
        int found = 0;

        for (int i = 0; i < HVT_RING_POLL_ITERS; i++) {
            hvt_rmb();
            if (ring->ent_head != ring->ent_tail) {
                found = 1;
                break;
            }
#if defined(__x86_64__)
            __asm__ __volatile__("pause");
#elif defined(__aarch64__)
            __asm__ __volatile__("yield");
#endif
        }

        if (!found) {
            /*
             * No work found after spinning. Set needs_kick so the guest knows
             * to signal us, then check once more to avoid a race where the
             * guest submitted between our last poll and setting the flag.
             */
            ring->needs_kick = 1;
            hvt_mb();

            if (ring->ent_head == ring->ent_tail) {
                /* Truly idle: block on notification fd.
                 * eventfd returns 8 bytes (KVM), pipe returns 1 byte
                 * (FreeBSD/OpenBSD).
                 */
                uint64_t val;
                ssize_t r = read(nfd, &val, sizeof(val));

                if (r <= 0) {
                    if (errno == EINTR)
                        continue;
                    break;
                }
            }

            ring->needs_kick = 0;
            hvt_wmb();
        }

        /*
         * Process all pending submissions with batched ent_head updates.
         */
        hvt_rmb();
        process_ring_commits(hvt, ring);
    }

    free(ta);
    return NULL;
}

static void kill_net_pthread(struct hvt *hvt, int status, void *cookie)
{
    (void)status;
    (void)cookie;
    struct hvt_b *hvb = hvt->b;

    io_thread_stop = 1;

#if defined(__linux__)
    if (hvb->kick_net_efd != -1) {
        uint64_t val = 1;
        (void)!write(hvb->kick_net_efd, &val, sizeof(val));
        pthread_join(hvb->io_thread_net, NULL);
        close(hvb->kick_net_efd);
        hvb->kick_net_efd = -1;
    }
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
    if (hvb->kick_net_pipe[1] != -1) {
        uint8_t byte = 1;
        (void)!write(hvb->kick_net_pipe[1], &byte, 1);
        pthread_join(hvb->io_thread_net, NULL);
        close(hvb->kick_net_pipe[0]);
        close(hvb->kick_net_pipe[1]);
        hvb->kick_net_pipe[0] = -1;
        hvb->kick_net_pipe[1] = -1;
    }
#endif
}

static int handle_cmdarg(char *cmdarg, struct mft *mft)
{
    enum { opt_net, opt_net_mac } which;

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
                                                 "%19s",
                    name, iface);
        if (rc != 2)
            return -1;
        struct mft_entry *e =
            mft_get_by_name(mft, name, MFT_DEV_NET_BASIC, NULL);
        if (e == NULL) {
            warnx("Resource not declared in manifest: '%s'", name);
            return -1;
        }
        int mtu = -1;
        int fd = tap_attach(iface, &mtu);
        if (fd < 0 || mtu < 0) {
            warnx("Could not attach interface: %s: %s", iface, strerror(errno));
            return -1;
        }

        /* e->u.net_basic.mac[] is set either by option or generated later by
         * setup().
         */
        e->u.net_basic.mtu = mtu;
        e->b.hostfd = fd;
        e->attached = true;
        module_in_use = true;
    } else if (which == opt_net_mac) {
        uint8_t mac[6];
        rc = sscanf(cmdarg,
                    "--net-mac:%" XSTR(MFT_NAME_MAX) "[A-Za-z0-9]="
                                                     "%02" SCNx8 ":%02" SCNx8
                                                     ":%02" SCNx8 ":"
                                                     "%02" SCNx8 ":%02" SCNx8
                                                     ":%02" SCNx8,
                    name, &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
        if (rc != 7)
            return -1;
        struct mft_entry *e =
            mft_get_by_name(mft, name, MFT_DEV_NET_BASIC, NULL);
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
        char no_mac[6] = {0};
        if (memcmp(mft->e[i].u.net_basic.mac, no_mac, sizeof no_mac) == 0)
            tap_attach_genmac(mft->e[i].u.net_basic.mac);
        assert(hvt_core_register_pollfd(mft->e[i].b.hostfd, i) == 0);
    }

    if (reserved_ring_gpa != 0) {
        struct hvt_b *hvb = hvt->b;
        struct hvt_ring *ring =
            (struct hvt_ring *)(hvt->mem + reserved_ring_gpa);
        hvb->net_ring_gpa = reserved_ring_gpa;
        int notify_fd = -1;

#if defined(__linux__)
        if (!hvb->has_ioeventfd)
            goto skip_ring;

        int efd = eventfd(0, EFD_CLOEXEC);
        if (efd == -1) {
            warn("eventfd() failed, falling back to hypercalls");
            goto skip_ring;
        }
        hvb->kick_net_efd = efd;

        struct kvm_ioeventfd ioev = {
            .datamatch = 0,
#if defined(__x86_64__)
            .addr = HVT_RING_KICK_PIO_BASE,
            .len = 4,
            .fd = efd,
            .flags = KVM_IOEVENTFD_FLAG_PIO,
#elif defined(__aarch64__)
            .addr = HVT_RING_KICK_MMIO_BASE,
            .len = 4,
            .fd = efd,
            .flags = 0,
#endif
        };

        if (ioctl(hvb->vmfd, KVM_IOEVENTFD, &ioev) == -1) {
            warn("KVM_IOEVENTFD failed, falling back to hypercalls");
            close(efd);
            hvb->kick_net_efd = -1;
            goto skip_ring;
        }
        notify_fd = efd;
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
        if (pipe(hvb->kick_net_pipe) == -1) {
            warn("pipe() failed, falling back to hypercalls");
            goto skip_ring;
        }
        notify_fd = hvb->kick_net_pipe[0];
#endif

        struct io_thread_arg *ta = malloc(sizeof(*ta));
        if (ta == NULL)
            err(1, "malloc");

        ta->hvt = hvt;
        ta->ring = ring;
        ta->notify_fd = notify_fd;
        ta->ready = 0;
        io_thread_stop = 0;

        if (pthread_create(&hvb->io_thread_net, NULL, io_thread_net_fn, ta) !=
            0) {
            warn("pthread_create() failed, falling back to hypercalls");
            free(ta);
#if defined(__linux__)
            close(hvb->kick_net_efd);
            hvb->kick_net_efd = -1;
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
            close(hvb->kick_net_pipe[0]);
            close(hvb->kick_net_pipe[1]);
            hvb->kick_net_pipe[0] = -1;
            hvb->kick_net_pipe[1] = -1;
#endif
            goto skip_ring;
        }

        /*
         * Wait for the I/O thread to signal that it is fully initialized.
         * This ensures all pthread runtime setup (TLS, stack, libc internals)
         * completes before hvt_drop_privileges() restricts syscalls via
         * pledge() on OpenBSD.
         */
        while (!ta->ready)
            ;

        assert(hvt_core_register_halt_hook(kill_net_pthread) == 0);
    skip_ring:;
    }

#if HVT_FREEBSD_ENABLE_CAPSICUM
    cap_rights_t rights;
    cap_rights_init(&rights, CAP_EVENT, CAP_WRITE, CAP_READ);
    if (cap_rights_limit(netfd, &rights) == -1)
        err(1, "cap_rights_limit() failed");
#endif

    return 0;
}

static const char *usage(void)
{
    return "--net:NAME=IFACE | @NN (attach tap at IFACE or at fd @NN as "
           "network NAME)\n"
           "  [ --net-mac:NAME=HWADDR ] (set HWADDR for network NAME)";
}

DECLARE_MODULE(net, .setup = setup, .handle_cmdarg = handle_cmdarg,
               .usage = usage)
