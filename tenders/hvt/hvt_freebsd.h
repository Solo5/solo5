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
 * hvt_freebsd.h: FreeBSD vmm(4) backend definitions.
 */

#ifndef HVT_HV_FREEBSD_H
#define HVT_HV_FREEBSD_H

#include <sys/types.h>
#include <sys/ioctl.h>
#include <machine/vmm.h>
#include <machine/vmm_dev.h>
#include <pthread.h>

#define VMM_USER   "nobody"
#define VMM_CHROOT "/var/empty"

struct hvt_b {
    char *vmname;
    int vmfd;
    struct vm_run vmrun;

    /* Ring I/O (pipe-based notification) */
    int kick_net_pipe[2];
    pthread_t io_thread_net;
    hvt_gpa_t net_ring_gpa;
};

#endif /* HVT_HV_FREEBSD_H */
