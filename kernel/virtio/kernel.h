/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a unikernel base layer.
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

#ifndef __VIRTIO_KERNEL_H__
#define __VIRTIO_KERNEL_H__

#include "../kernel.h"
#include "multiboot.h"

/* alignment macros */
#define ALIGN_4K __attribute__((aligned(0x1000)))
#define ALIGN_64_BIT __attribute__((aligned(0x8)))

/* We have already set up the GDT for the kernel.  Here are the
 * descriptor numbers (useful for when the kernel sets up the IDT)
 */
#define GDT_NUM_ENTRIES 6
#define GDT_DESC_NULL 0
#define GDT_DESC_CODE 1
/* 2 == unused / 32-bit bootstrap */
#define GDT_DESC_DATA 3
#define GDT_DESC_TSS_LO  4
#define GDT_DESC_TSS_HI  5
#define GDT_DESC_TSS  GDT_DESC_TSS_LO
#define GDT_DESC_OFFSET(n) ((n) * 0x8)

/* We have already loaded a "known good" stack in the TSS */
#define TSS_IST_INDEX 0x1

void mem_init(struct multiboot_info *mb);
uint64_t mem_max_addr(void);

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

/* cpu.S: low level cpu functions */
void cpu_tss_load(uint16_t);
void cpu_idt_load(uint64_t);
void cpu_gdt_load(uint64_t);
void cpu_sse_enable(void);

/* intr.c: interrupt handling */
void intr_init(void);
void intr_enable(void);
void intr_disable(void);
void intr_register_irq(unsigned irq, int (*handler)(void *), void *arg);
extern int intr_depth;

/* platform_intr.c: platform-specific interrupt handling */
void platform_intr_init(void);
void platform_intr_clear_irq(unsigned irq);
void platform_intr_mask_irq(unsigned irq);
void platform_intr_ack_irq(unsigned irq);

/* accessing devices via port space */
static inline void outb(uint16_t port, uint8_t v)
{
    __asm__ __volatile__("outb %0,%1" : : "a" (v), "dN" (port));
}
static inline void outw(uint16_t port, uint16_t v)
{
    __asm__ __volatile__("outw %0,%1" : : "a" (v), "dN" (port));
}
static inline void outl(uint16_t port, uint32_t v)
{
    __asm__ __volatile__("outl %0,%1" : : "a" (v), "dN" (port));
}
static inline uint8_t inb(uint16_t port)
{
    uint8_t v;

    __asm__ __volatile__("inb %1,%0" : "=a" (v) : "dN" (port));
    return v;
}
static inline uint16_t inw(uint16_t port)
{
    uint16_t v;

    __asm__ __volatile__("inw %1,%0" : "=a" (v) : "dN" (port));
    return v;
}
static inline uint32_t inl(uint16_t port)
{
    uint32_t v;

    __asm__ __volatile__("inl %1,%0" : "=a" (v) : "dN" (port));
    return v;
}

static inline uint64_t inq(uint16_t port_lo)
{
    uint16_t port_hi = port_lo + 4;
    uint32_t lo, hi;

    __asm__ __volatile__("inl %1,%0" : "=a" (lo) : "dN" (port_lo));
    __asm__ __volatile__("inl %1,%0" : "=a" (hi) : "dN" (port_hi));

    return ((uint64_t)lo) | ((uint64_t)hi << 32);
}

#define atomic_printf(x...) do {                      \
        intr_disable();                               \
        printf(x);                                    \
        intr_enable();                                \
    } while (0)


#endif
