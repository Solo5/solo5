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

/* 8 bits for bus number, 5 bits for devices, 3 bits for functions */
#define PCI_MAX_BUSES (1 << 8)
#define PCI_MAX_DEVICES (1 << 5)
#define PCI_MAX_FUNCTIONS (1 << 3)

#define PCI_BUS_SHIFT    (16)
#define PCI_DEVICE_SHIFT (11)
#define PCI_FUNCTION_SHIFT (8)
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

void (*pci_acpi_power_off)(void) = NULL;

#define PCI_CONF_READ(type, ret, a, s) do {                          \
    uint32_t _conf_data;                                             \
    outl(PCI_CONFIG_ADDR, (a) | PCI_CONF_##s);                       \
    _conf_data = ((inl(PCI_CONFIG_DATA) >> PCI_CONF_##s##_SHFT)      \
                  & PCI_CONF_##s##_MASK);                            \
    *(ret) = (type) _conf_data;                                      \
} while (0)

static uint32_t net_devices_found;
static uint32_t blk_devices_found;

#define PCI_CONF_SUBSYS_NET 1
#define PCI_CONF_SUBSYS_BLK 2

static void virtio_config(struct pci_config_info *pci)
{
    /* we only support one net device and one blk device */
    switch (pci->subsys_id) {
    case PCI_CONF_SUBSYS_NET:
        log(INFO, "Solo5: PCI:%02x:%02x.%02x: virtio-net device, base=0x%x, irq=%u\n",
            pci->bus, pci->dev, pci->fun, pci->base, pci->irq);
        if (!net_devices_found++)
            virtio_config_network(pci);
        else
            log(WARN, "Solo5: PCI:%02x:%02x.%02x: not configured\n", pci->bus,
                pci->dev, pci->fun);
        break;
    case PCI_CONF_SUBSYS_BLK:
        log(INFO, "Solo5: PCI:%02x:%02x.%02x: virtio-block device, base=0x%x, irq=%u\n",
            pci->bus, pci->dev, pci->fun, pci->base, pci->irq);
        if (!blk_devices_found++)
            virtio_config_block(pci);
        else
            log(WARN, "Solo5: PCI:%02x:%02x.%02x: not configured\n", pci->bus,
                pci->dev, pci->fun);
        break;
    default:
        log(WARN, "Solo5: PCI:%02x:%02x.%02x: unknown virtio device (0x%x)\n",
            pci->bus, pci->dev, pci->fun, pci->subsys_id);
        return;
    }
}

static void poweroff_noop(void) { }

/* According to the Google Compute Engine docs, guest OSes can gain access to
   ACPI features by supporting the Intel PIIX4. We use it to implement power off. */

/* This PIIX4 poweroff technique is learned from the Linux kernel.
   https://github.com/torvalds/linux/blob/master/drivers/power/reset/piix4-poweroff.c */

#define DEVICE_ID_INTEL_PIIX4  0x7113

static struct pci_config_info piix4_pci_dev;
#define PIIX4_SUSPEND_MAGIC			0x00120002

enum piix4_pm_io_reg {
	PIIX4_FUNC3IO_PMSTS			= 0x00,
#define PIIX4_FUNC3IO_PMSTS_PWRBTN_STS		  (1 << 8)
	PIIX4_FUNC3IO_PMCNTRL			= 0x04,
#define PIIX4_FUNC3IO_PMCNTRL_SUS_EN		    (1 << 13)
#define PIIX4_FUNC3IO_PMCNTRL_SUS_TYP_SOFF	(0x0 << 10)
};

/*
#define PCI_OP_WRITE(size, type, len) \
int noinline pci_bus_write_config_##size \
	(struct pci_bus *bus, unsigned int devfn, int pos, type value)	\
{									\
	int res;							\
	unsigned long flags;						\
	if (PCI_##size##_BAD) return PCIBIOS_BAD_REGISTER_NUMBER;	\
	pci_lock_config(flags);						\
	res = bus->ops->write(bus, devfn, pos, len, value);		\
	pci_unlock_config(flags);					\
	return res;							\
}
*/

static void poweroff_piix4(void)
{
    uint16_t sts;

    /* In a full PCI implementation we would calculate this stuff on enumeration,
       but rather than introduce potential bugs at startup probing for these
       features, lets leave it for poweroff. */
    log(WARN, "Solo5: initiating PIIX4 ACPI poweroff\n");

    /* Ensure the power button status is clear */
    while (1) {
        sts = inw(piix4_pci_dev.base + PIIX4_FUNC3IO_PMSTS);
        if (!(sts & PIIX4_FUNC3IO_PMSTS_PWRBTN_STS))
            break;
        outw(piix4_pci_dev.base + PIIX4_FUNC3IO_PMSTS, sts);
    }
    log(WARN, "Solo5: button status is clear\n");

    /* Enable entry to suspend */
    outw(
        piix4_pci_dev.base + PIIX4_FUNC3IO_PMCNTRL,
        PIIX4_FUNC3IO_PMCNTRL_SUS_TYP_SOFF | PIIX4_FUNC3IO_PMCNTRL_SUS_EN
        );

    /* If the special cycle occurs too soon this doesn't work... */
    cpu_wasteful_milli_sleep(10);

    log(WARN, "Solo5: milli_sleep 10 complete\n");

    /* The PIIX4 will enter the suspend state only after seeing a special cycle
       with the correct magic data on the PCI bus. Generate that cycle now. */
    /*
    pci_bus_write_config_dword(
        piix4_pci_dev,
        (0x1f << 3 | 0x7),
        0,
        PIIX4_SUSPEND_MAGIC);
     */
    /* Give the system some time to power down, then error */
    cpu_wasteful_milli_sleep(1000);
    log(WARN, "Solo5: PIIX4 ACPI poweroff failed!\n");
}


void (*pci_acpi_poweroff)(void) = poweroff_noop;

static void intel_config(struct pci_config_info *pci)
{
    switch (pci->device_id) {
        case DEVICE_ID_INTEL_PIIX4:
            log(INFO, "Solo5: PCI:%02x:%02x.%02x: Intel 82371AB/EB/MB PIIX4 ACPI, irq=%u, base=0x%x\n",
                pci->bus, pci->dev, pci->fun, pci->irq, pci->base);
            memcpy(&piix4_pci_dev, pci, sizeof(struct pci_config_info));
            pci_acpi_poweroff = poweroff_piix4;
            break;
        default:
            log(WARN, "Solo5: PCI:%02x:%02x.%02x: unknown Intel device; device_id:0x%x, subsys_id: 0x%x\n",
                pci->bus, pci->dev, pci->fun, pci->device_id, pci->subsys_id);
            break;
    }
}

#define VENDOR_INTEL 0x8086
#define VENDOR_QUMRANET_VIRTIO 0x1af4

void pci_enumerate(void)
{
    uint32_t bus;
    uint8_t dev;
    uint8_t fun;

    for (bus = 0; bus < PCI_MAX_BUSES; bus++) {
        for (dev = 0; dev < PCI_MAX_DEVICES; dev++) {
            for (fun = 0; fun < PCI_MAX_FUNCTIONS; fun++) {
                uint32_t config_addr, config_data;
                uint16_t vendor_id;
                struct pci_config_info pci;

                config_addr = (PCI_ENABLE_BIT)
                  | (bus << PCI_BUS_SHIFT)
                  | (dev << PCI_DEVICE_SHIFT)
                  | (fun << PCI_FUNCTION_SHIFT);

                outl(PCI_CONFIG_ADDR, config_addr);
                config_data = inl(PCI_CONFIG_DATA);

                vendor_id = config_data & 0xffff;
                if (vendor_id == 0xffff) {
                    /* This means there's no device here. */
                    continue;
                }
                pci.bus = bus;
                pci.dev = dev;
                pci.fun = fun;
                pci.vendor_id = vendor_id;
                pci.device_id = (config_data >> 16) & 0xffff;

                PCI_CONF_READ(uint16_t, &pci.subsys_id, config_addr, SUBSYS_ID);
                PCI_CONF_READ(uint16_t, &pci.base, config_addr, IOBAR);
                PCI_CONF_READ(uint8_t, &pci.irq, config_addr, IRQ);

                if (pci.vendor_id == VENDOR_QUMRANET_VIRTIO) {
                    virtio_config(&pci);
                } else if (pci.vendor_id == VENDOR_INTEL) {
                    intel_config(&pci);
                }
            }
        }
    }
}
