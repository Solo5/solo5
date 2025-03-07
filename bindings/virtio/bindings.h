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
 * bindings.h: Solo5 bindings, virtio implementation additions.
 *
 * This header file includes (supersedes) the common bindings.h for the virtio
 * implementation.
*/

#ifndef __VIRTIO_BINDINGS_H__
#define __VIRTIO_BINDINGS_H__

#include "../bindings.h"
#include "multiboot.h"
#include "elf_abi.h"

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
uint64_t rtc_gettimeofday(void);
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

int pci_enumerate(void);

int virtio_config_network(struct pci_config_info *, solo5_handle_t);
int virtio_config_block(struct pci_config_info *, solo5_handle_t);

#endif /* __VIRTIO_BINDINGS_H__ */
