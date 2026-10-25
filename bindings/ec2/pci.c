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
 * PCI enumeration through the legacy configuration ports (EC2 has no
 * MCFG). Finds the NVMe and ENA devices and maps their registers uncached.
 */

#include "bindings.h"

#define PCI_CONFIG_ADDR 0xcf8
#define PCI_CONFIG_DATA 0xcfc

#define PCI_MAX_BUSES     256
#define PCI_MAX_DEVICES   32
#define PCI_MAX_FUNCTIONS 8
#define PCI_NUM_BARS      6

#define PCI_ENABLE_BIT     (1u << 31)
#define PCI_BUS_SHIFT      16
#define PCI_DEVICE_SHIFT   11
#define PCI_FUNCTION_SHIFT 8

/* Configuration space */
#define PCI_ID             0x00
#define PCI_COMMAND        0x04
#define PCI_CLASS_REVISION 0x08
#define PCI_HEADER_TYPE    0x0c /* byte 2 of this dword */
#define PCI_BAR0           0x10

#define PCI_VENDOR_NONE          0xffff
#define PCI_COMMAND_MASK         0xffff /* the dword's upper half is status */
#define PCI_COMMAND_MEMORY       (1u << 1)
#define PCI_COMMAND_MASTER       (1u << 2)
#define PCI_HEADER_MULTIFUNCTION 0x80

#define PCI_BAR_IO        (1u << 0)
#define PCI_BAR_TYPE_MASK (3u << 1)
#define PCI_BAR_TYPE_64   (2u << 1)
#define PCI_BAR_ADDR_MASK (~0xfu)

#define PCI_CLASS_NVME 0x010802

#define AMAZON_VENDOR_ID  0x1d0f
#define AMAZON_ENA_VF     0xec20
#define AMAZON_ENA_LLQ_VF 0xec21

/* Page tables: boot.S identity-maps 0-4 GiB with 2 MiB pages */
#define PTE_PWT            (1ULL << 3)
#define PTE_PCD            (1ULL << 4)
#define PTE_PS             (1ULL << 7)
#define PTE_ADDR_MASK      0x000ffffffffff000ULL
#define PT_INDEX(a, shift) (((a) >> (shift)) & 511)
#define PML4_SHIFT         39
#define PDPT_SHIFT         30
#define PD_SHIFT           21
#define PAGE_2M            (1ULL << PD_SHIFT)
#define IDENTITY_MAP_END   (1ULL << 32)

static uint32_t pci_addr(unsigned bus, unsigned dev, unsigned fn, unsigned off)
{
    return PCI_ENABLE_BIT | (bus << PCI_BUS_SHIFT) | (dev << PCI_DEVICE_SHIFT) |
           (fn << PCI_FUNCTION_SHIFT) | (off & 0xfc);
}

uint32_t pci_cfg_read32(const struct pci_dev *d, unsigned off)
{
    outl(PCI_CONFIG_ADDR, pci_addr(d->bus, d->dev, d->fn, off));
    return inl(PCI_CONFIG_DATA);
}

void pci_cfg_write32(const struct pci_dev *d, unsigned off, uint32_t v)
{
    outl(PCI_CONFIG_ADDR, pci_addr(d->bus, d->dev, d->fn, off));
    outl(PCI_CONFIG_DATA, v);
}

static inline uint64_t read_cr3(void)
{
    uint64_t v;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(v));
    return v;
}

/* Sets PCD|PWT on the 2 MiB page holding (phys) in boot.S's identity map. */
static int map_uncached(uint64_t phys)
{
    if (phys < PAGE_2M || phys >= IDENTITY_MAP_END)
        return -1;
    uint64_t *pml4 = (uint64_t *)(read_cr3() & PTE_ADDR_MASK);
    uint64_t *pdpt =
        (uint64_t *)(pml4[PT_INDEX(phys, PML4_SHIFT)] & PTE_ADDR_MASK);
    uint64_t *pd =
        (uint64_t *)(pdpt[PT_INDEX(phys, PDPT_SHIFT)] & PTE_ADDR_MASK);
    uint64_t *pde = &pd[PT_INDEX(phys, PD_SHIFT)];
    if (!(*pde & PTE_PS))
        return -1;
    if ((*pde & (PTE_PCD | PTE_PWT)) != (PTE_PCD | PTE_PWT)) {
        *pde |= PTE_PCD | PTE_PWT;
        __asm__ __volatile__("invlpg (%0)"
                             :
                             : "r"(phys & ~(PAGE_2M - 1))
                             : "memory");
    }
    return 0;
}

uint64_t pci_map_bar(const struct pci_dev *d, unsigned bar)
{
    uint32_t lo = pci_cfg_read32(d, PCI_BAR0 + 4 * bar);
    if (lo & PCI_BAR_IO)
        return 0;
    uint64_t addr = lo & PCI_BAR_ADDR_MASK;
    if ((lo & PCI_BAR_TYPE_MASK) == PCI_BAR_TYPE_64 && bar < PCI_NUM_BARS - 1)
        addr |= (uint64_t)pci_cfg_read32(d, PCI_BAR0 + 4 * (bar + 1)) << 32;
    if (addr == 0) {
        log(WARN, "Solo5: PCI:%02x:%02x.%x: BAR%u not assigned\n", d->bus,
            d->dev, d->fn, bar);
        return 0;
    }
    if (map_uncached(addr) != 0) {
        log(WARN,
            "Solo5: PCI:%02x:%02x.%x: BAR%u at 0x%llx is outside the "
            "identity map\n",
            d->bus, d->dev, d->fn, bar, (unsigned long long)addr);
        return 0;
    }
    uint32_t cmd = pci_cfg_read32(d, PCI_COMMAND);
    pci_cfg_write32(d, PCI_COMMAND,
                    (cmd & PCI_COMMAND_MASK) | PCI_COMMAND_MEMORY |
                        PCI_COMMAND_MASTER);
    return addr;
}

static void pci_found(const struct pci_dev *d)
{
    if (d->class == PCI_CLASS_NVME) {
        if (nvme_attach(d) != 0)
            log(WARN, "Solo5: PCI:%02x:%02x.%x: NVMe controller not usable\n",
                d->bus, d->dev, d->fn);
    } else if (d->vendor == AMAZON_VENDOR_ID &&
               (d->device == AMAZON_ENA_VF || d->device == AMAZON_ENA_LLQ_VF)) {
        if (ena_attach(d) != 0)
            log(WARN, "Solo5: PCI:%02x:%02x.%x: ENA device not usable\n",
                d->bus, d->dev, d->fn);
    }
}

int pci_enumerate(void)
{
    for (unsigned bus = 0; bus < PCI_MAX_BUSES; bus++) {
        for (unsigned dev = 0; dev < PCI_MAX_DEVICES; dev++) {
            for (unsigned fn = 0; fn < PCI_MAX_FUNCTIONS; fn++) {
                struct pci_dev d = {.bus = bus, .dev = dev, .fn = fn};
                uint32_t id = pci_cfg_read32(&d, PCI_ID);
                if ((id & 0xffff) == PCI_VENDOR_NONE) {
                    if (fn == 0)
                        break;
                    continue;
                }
                d.vendor = id & 0xffff;
                d.device = id >> 16;
                d.class = pci_cfg_read32(&d, PCI_CLASS_REVISION) >> 8;
                pci_found(&d);

                uint8_t hdr = pci_cfg_read32(&d, PCI_HEADER_TYPE) >> 16;
                if (fn == 0 && !(hdr & PCI_HEADER_MULTIFUNCTION))
                    break;
            }
        }
    }
    return 0;
}
