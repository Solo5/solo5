#ifndef __VIRTIO_KERNEL_H__
#define __VIRTIO_KERNEL_H__

#include "../kernel.h"
#include "multiboot.h"

void mem_init(struct multiboot_info *mb);

/* serial.c: console output for debugging */
void serial_init(void);
void serial_putc(char a);

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

uint8_t *virtio_net_pkt_get(int *size);  /* get a pointer to recv'd data */
void virtio_net_pkt_put(void);      /* we're done with recv'd data */
int virtio_net_xmit_packet(void *data, int len);
int virtio_net_pkt_poll(void);      /* test if packet(s) are available */

#endif
