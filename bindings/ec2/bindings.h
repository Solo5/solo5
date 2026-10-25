/*
 * Copyright (c) 2015-2020 Contributors as noted in the AUTHORS file
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
 * bindings.h: Solo5 bindings, ec2 implementation additions.
 *
 * This header file includes (supersedes) the virtio bindings.h, whose boot,
 * console, clock and platform code ec2 shares.
 */

#ifndef __EC2_BINDINGS_H__
#define __EC2_BINDINGS_H__

#include "../virtio/bindings.h"

/* start.c */
extern struct mft *ec2_manifest;
const char *ec2_block_manifest_name(const char *devname);

/* pci.c */
struct pci_dev {
    uint8_t bus, dev, fn;
    uint16_t vendor, device;
    uint32_t class; /* class, subclass, prog-if */
};

uint32_t pci_cfg_read32(const struct pci_dev *d, unsigned off);
void pci_cfg_write32(const struct pci_dev *d, unsigned off, uint32_t v);

/* Maps memory BAR (bar) uncached and enables bus mastering; 0 on failure. */
uint64_t pci_map_bar(const struct pci_dev *d, unsigned bar);

/* nvme.c */
int nvme_attach(const struct pci_dev *d);

/* ena.c */
int ena_attach(const struct pci_dev *d);

static inline void cpu_relax(void)
{
    __asm__ __volatile__("pause" ::: "memory");
}

/* As virtio_mb(): ring entries before the doorbell write */
#define ec2_mb() __asm__ __volatile__("mfence" ::: "memory")

/* MMIO */
static inline uint32_t mmio_read32(uint64_t a)
{
    return *(volatile uint32_t *)a;
}

static inline void mmio_write32(uint64_t a, uint32_t v)
{
    *(volatile uint32_t *)a = v;
}

static inline uint64_t mmio_read64(uint64_t a)
{
    return mmio_read32(a) | ((uint64_t)mmio_read32(a + 4) << 32);
}

static inline void mmio_write64(uint64_t a, uint64_t v)
{
    mmio_write32(a, (uint32_t)v);
    mmio_write32(a + 4, (uint32_t)(v >> 32));
}

#endif /* __EC2_BINDINGS_H__ */
