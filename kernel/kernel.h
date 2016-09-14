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

#ifndef __KERNEL_H__
#define __KERNEL_H__

#include "solo5.h"

/* This is the main header file for everything in the kernel */


/* common headers from the x86_64-elf gcc cross compiler */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

/* alignment macros */
#define ALIGN_4K __attribute__((aligned(0x1000)))
#define ALIGN_64_BIT __attribute__((aligned(0x8)))

/* memory defines */
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define PAGE_MASK ~(0xfff)

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

/* convenient macro stringification */
#define STR_EXPAND(y) #y
#define STR(x) STR_EXPAND(x)

/* cpu.S: low-level CPU functions */
void cpu_halt(void) __attribute__((noreturn));
void cpu_tss_load(uint16_t);
void cpu_idt_load(uint64_t);
void cpu_gdt_load(uint64_t);
void cpu_sse_enable(void);
uint64_t cpu_rdtsc(void);

/* intr.c: interrupt handling */
void interrupts_init(void);
void interrupts_enable(void);
void interrupts_disable(void);
void intr_clear_irq(unsigned irq);
void intr_mask_irq(unsigned irq);
void intr_ack_irq(unsigned);
void intr_register_irq(unsigned, int (*handler)(void *), void *);
extern int spldepth;

/* mem.c: low-level page alloc routines */
uint64_t mem_max_addr(void);
void *sbrk(intptr_t increment);

/* malloc.c: memory allocation */
void *malloc(size_t bytes);
void  free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void *memalign(size_t alignment, size_t bytes);

/* time.c: clocksource */
void time_init(void);

/* ee_printf.c: a third-party printf slightly modified and with
 *              snprintf added
 */
int printf(const char *fmt, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

/* lib.c: expect this to grow... */
void *memset(void *ptr, uint8_t c, size_t size);
void *memcpy(void *dst, const void *src, size_t size);
void *memmove(void *dst, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
char *strcpy(char *dst, const char *src);
size_t strlen(const char *s);

/* pci.c: only enumerate for now */
void pci_enumerate(void);

/* virtio.c: mostly net for now */
void virtio_config_network(uint16_t base, unsigned irq);
void virtio_config_block(uint16_t base, unsigned irq);

uint8_t *virtio_net_pkt_get(int *size);  /* get a pointer to recv'd data */
void virtio_net_pkt_put(void);      /* we're done with recv'd data */
int virtio_net_xmit_packet(void *data, int len);
int virtio_net_pkt_poll(void);      /* test if packet(s) are available */

/* low_level.c: specifics for ukvm or virito target */
void low_level_exit(void);
int low_level_puts(char *buf, int n);

void low_level_interrupts_init(void);

/* pvclock.c: KVM paravirtualized clock */
int pvclock_init(void);
uint64_t pvclock_monotonic(void);
uint64_t pvclock_epochoffset(void);

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

static inline uint64_t mul64_32(uint64_t a, uint32_t b)
{
    uint64_t prod;

    __asm__ (
        "mul %%rdx ; "
        "shrd $32, %%rdx, %%rax"
        : "=a" (prod)
        : "0" (a), "d" ((uint64_t)b)
    );

    return prod;
}

/* compiler-only memory "barrier" */
#define cc_barrier() __asm__ __volatile__("" : : : "memory")

#define PANIC(x...) do {                                   \
        printf("PANIC: %s:%d\n", __FILE__, __LINE__);      \
        printf(x);                                         \
        cpu_halt();                                        \
    } while (0)

#define assert(e) do {                              \
        if (!(e))                                   \
            PANIC("assertion failed: \"%s\"", #e);  \
    } while (0)

#define dprintf(x...) do {                      \
        if (dbg) {                              \
            printf(x);                          \
        }                                       \
    } while (0)

/* should only use outside of interrupt context */
#define atomic_printf(x...) do {                      \
        interrupts_disable();                         \
        printf(x);                                    \
        interrupts_enable();                          \
    } while (0)


#define NSEC_PER_SEC	1000000000ULL

#endif
