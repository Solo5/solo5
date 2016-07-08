/* Copyright (c) 2015, IBM
 * Author(s): Dan Williams <djwillia@us.ibm.com>
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

#include "kernel.h"

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

#define PCI_CONF_INTERRUPT_LINE      0x3c
#define PCI_CONF_INTERRUPT_LINE_SHFT 0x0
#define PCI_CONF_INTERRUPT_LINE_MASK 0xff

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


struct pci_config_info {
    uint16_t device_id;
    uint16_t iobar;
    uint8_t interrupt_line;
};

uint32_t net_devices_found;
uint32_t blk_devices_found;

#define PCI_CONF_SUBSYS_NET 1
#define PCI_CONF_SUBSYS_BLK 2

static void virtio_config(uint32_t config_addr)
{
    struct pci_config_info pci;
    int i;
    int n = sizeof(solo5_devices) / sizeof(solo5_device);

    PCI_CONF_READ(uint16_t, &pci.device_id, config_addr, SUBSYS_ID);
    PCI_CONF_READ(uint16_t, &pci.iobar, config_addr, IOBAR);
    PCI_CONF_READ(uint8_t, &pci.interrupt_line, config_addr, INTERRUPT_LINE);

    printf("virtio_config: device_id=%x, interrupt_line=%x\n", pci.device_id,
            pci.interrupt_line);

    /* FIXME: this assumes that all devices are PCI based, and that all of */
    /* them use the same interrupt line */
    for (i = 0; i < n; i++)
        solo5_devices[i].irq_num = pci.interrupt_line;

    /* we only support one net device and one blk device */
    switch (pci.device_id) {
    case PCI_CONF_SUBSYS_NET:
        if (!net_devices_found++)
            virtio_config_network(pci.iobar);
        break;
    case PCI_CONF_SUBSYS_BLK:
        if (!blk_devices_found++)
            virtio_config_block(pci.iobar);
        break;
    default:
        printf("Found unknown virtio device!\n");
        return;
    }

    irq_clear(pci.interrupt_line);
}


#define VENDOR_QUMRANET_VIRTIO 0x1af4

void pci_enumerate(void)
{
    uint32_t bus;
    uint32_t device;

    for (bus = 0; bus < PCI_MAX_BUSES; bus++) {
        for (device = 0; device < PCI_MAX_DEVICES; device++) {
            uint32_t config_addr, config_data;
            uint16_t vendor_id;

            config_addr = (PCI_ENABLE_BIT)
                | (bus << PCI_BUS_SHIFT)
                | (device << PCI_DEVICE_SHIFT);

            outl(PCI_CONFIG_ADDR, config_addr);
            config_data = inl(PCI_CONFIG_DATA);

            vendor_id = config_data & 0xffff;

            if (vendor_id == VENDOR_QUMRANET_VIRTIO) {
                virtio_config(config_addr);
                if (net_devices_found + blk_devices_found == 2)
                    goto done_pci_enum;
            }
        }
    }
 done_pci_enum:
    return;
}

