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

#include "bindings.h"

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

/* 8 bits for bus number, 5 bits for devices */
#define PCI_MAX_BUSES (1 << 8)
#define PCI_MAX_DEVICES (1 << 5)

#define PCI_BUS_SHIFT    (16)
#define PCI_DEVICE_SHIFT (11)
#define PCI_ENABLE_BIT   (1 << 31)

#define PCI_CONF_SUBSYS_ID       0x2c
#define PCI_CONF_SUBSYS_ID_SHFT  16
#define PCI_CONF_SUBSYS_ID_MASK  0xffff

#define PCI_CONF_IRQ      0x3c
#define PCI_CONF_IRQ_SHFT 0x0
#define PCI_CONF_IRQ_MASK 0xff

#define PCI_CONF_IOBAR      0x10
#define PCI_CONF_IOBAR_SHFT 0x0
#define PCI_CONF_IOBAR_MASK ~0x3


#define PCI_CONF_READ(type, ret, a, s) do {                          \
    uint32_t _conf_data;                                             \
    outl(PCI_CONFIG_ADDR, (a) | PCI_CONF_##s);                       \
    _conf_data = ((inl(PCI_CONFIG_DATA) >> PCI_CONF_##s##_SHFT)      \
                  & PCI_CONF_##s##_MASK);                            \
    *(ret) = (type) _conf_data;                                      \
} while (0)


/* in tenders/common/mft.c:
 * The manifest must contain at least one entry, and the first entry must
 * be of type MFT_RESERVED_FIRST with an empty name.
 * -> Therefore we need to start mft_index at 1
 */
static solo5_handle_t mft_index = 1;
#define PCI_CONF_SUBSYS_NET 1
#define PCI_CONF_SUBSYS_BLK 2

/*
 * mft_index tracks the order in which devices are found (given by the
 * PCI slots). This is used to map found devices to devices in the manifest.
 */
static int virtio_config(struct pci_config_info *pci)
{
    switch (pci->subsys_id) {
    case PCI_CONF_SUBSYS_NET:
        log(INFO, "Solo5: PCI:%02x:%02x: virtio-net device, base=0x%x, irq=%u\n",
            pci->bus, pci->dev, pci->base, pci->irq);
        if (virtio_config_network(pci, mft_index) != 0)
            return -1;
        mft_index++;
        break;
    case PCI_CONF_SUBSYS_BLK:
        log(INFO, "Solo5: PCI:%02x:%02x: virtio-block device, base=0x%x, irq=%u\n",
            pci->bus, pci->dev, pci->base, pci->irq);
        if (virtio_config_block(pci, mft_index) != 0)
            return -1;
        mft_index++;
        break;
    default:
        /* Most likely this is something like a virtio_scsi device
           that we can not handle. Let's not handle nor count it
           in pci_virtio_index. */
        log(WARN, "Solo5: PCI:%02x:%02x: unknown virtio device (0x%x)\n",
            pci->bus, pci->dev, pci->subsys_id);
    }
    return 0;
}

#define VENDOR_QUMRANET_VIRTIO 0x1af4

/*
 * Every PCI device found in pci_enumerate is attempted to be assigned to a
 * device from the application manifest in the order in which they are found. This
 * order is given by the PCI slot number which is controlled in
 * solo5-virtio-run.sh.
 * 
 * This funtion can fail if the type of a device just found does not match the
 * type of the next expected device in the manifest. Devices whose type (e.g.,
 * virtio_scsi) is unknown is just skipped.
 * 
 * Returns 0 on success, -1 otherwise.
 */
int pci_enumerate(void)
{
    uint32_t bus;
    uint8_t dev;

    for (bus = 0; bus < PCI_MAX_BUSES; bus++) {
        for (dev = 0; dev < PCI_MAX_DEVICES; dev++) {
            uint32_t config_addr, config_data;
            struct pci_config_info pci;

            config_addr = (PCI_ENABLE_BIT)
                | (bus << PCI_BUS_SHIFT)
                | (dev << PCI_DEVICE_SHIFT);

            outl(PCI_CONFIG_ADDR, config_addr);
            config_data = inl(PCI_CONFIG_DATA);

            pci.bus = bus;
            pci.dev = dev;
            pci.vendor_id = config_data & 0xffff;

            if (pci.vendor_id == VENDOR_QUMRANET_VIRTIO) {
                PCI_CONF_READ(uint16_t, &pci.subsys_id, config_addr, SUBSYS_ID);
                PCI_CONF_READ(uint16_t, &pci.base, config_addr, IOBAR);
                PCI_CONF_READ(uint8_t, &pci.irq, config_addr, IRQ);

                if (virtio_config(&pci) != 0)
                    return -1;
            }
        }
    }
    return 0;
}
