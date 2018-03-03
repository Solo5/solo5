/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a unikernel base layer.
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

#ifndef __VIRTIO_KERNEL_H__
#define __VIRTIO_KERNEL_H__

#include "../kernel.h"
#include "multiboot.h"

/* serial.c: console output for debugging */
void serial_init(void);
void serial_putc(char a);

void time_init(void);

/* pvclock.c: KVM paravirtualized clock */
int pvclock_init(void);
uint64_t pvclock_monotonic(void);
uint64_t pvclock_epochoffset(void);

/* tscclock.c: TSC/PIT-based clock and sleep */
int tscclock_init(void);
uint64_t tscclock_monotonic(void);
uint64_t tscclock_epochoffset(void);
void cpu_block(uint64_t until);

/* pci.c: only enumerate for now */
struct pci_config_info {
    uint8_t bus;
    uint8_t dev;
    uint16_t vendor_id;
    uint16_t subsys_id;
    uint16_t base;
    uint8_t irq;
};

void pci_enumerate(void);

/* virtio.c: mostly net for now */
void virtio_config_network(struct pci_config_info *);
void virtio_config_block(struct pci_config_info *);

uint8_t *virtio_net_pkt_get(size_t *size);  /* get a pointer to recv'd data */
void virtio_net_pkt_put(void);      /* we're done with recv'd data */
int virtio_net_xmit_packet(const void *data, size_t len);
int virtio_net_pkt_poll(void);      /* test if packet(s) are available */

#endif
