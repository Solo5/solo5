/* 
 * Copyright (c) 2015-2018 Contributors as noted in the AUTHORS file
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

#ifndef VIRTIO_PCI_H
#define VIRTIO_PCI_H

/* virtio config space layout */
#define VIRTIO_PCI_HOST_FEATURES        0    /* 32-bit r/o */
#define VIRTIO_PCI_GUEST_FEATURES       4    /* 32-bit r/w */
#define VIRTIO_PCI_QUEUE_PFN            8    /* 32-bit r/w */
#define VIRTIO_PCI_QUEUE_SIZE           12   /* 16-bit r/o */
#define VIRTIO_PCI_QUEUE_SEL            14   /* 16-bit r/w */
#define VIRTIO_PCI_QUEUE_NOTIFY         16   /* 16-bit r/w */

/* How many bits to shift physical queue address written to QUEUE_PFN.
 * 12 is historical, and due to x86 page size.
 */
#define VIRTIO_PCI_QUEUE_ADDR_SHIFT     12


/* The status register lets us tell the device where we are in
 * initialization
 */
#define VIRTIO_PCI_STATUS               18   /* 8-bit r/w */
#define VIRTIO_PCI_STATUS_ACK           0x1  /* we recognize device as virtio */
#define VIRTIO_PCI_STATUS_DRIVER        0x2  /* we want to drive it */
#define VIRTIO_PCI_STATUS_DRIVER_OK     0x4  /* initialization is complete */
#define VIRTIO_PCI_STATUS_FAIL          0x80 /* tell device something's wrong */

/* Reading the value will return the current contents of the interrupt
 * status register and will also clear it.  This is effectively a
 * read-and-acknowledge.
 */
#define VIRTIO_PCI_ISR                  19   /* 8-bit r/o */
#define VIRTIO_PCI_ISR_HAS_INTR         0x1  /* interrupt is for this device */
#define VIRTIO_PCI_ISR_CONFIG           0x2  /* config change bit */

/* xxx assuming msi is not configured */
#define VIRTIO_PCI_CONFIG_OFF           20

#endif
