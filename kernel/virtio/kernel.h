#ifndef __VIRTIO_KERNEL_H__
#define __VIRTIO_KERNEL_H__

#include "../kernel.h"
#include "../../loader/multiboot.h"
#include "../../loader/loader_info.h"

void mem_init(struct multiboot_info *mb);

/* serial.c: console output for debugging */
void serial_init(void);
void serial_putc(char a);


#endif
