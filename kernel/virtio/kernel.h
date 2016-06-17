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

#endif
