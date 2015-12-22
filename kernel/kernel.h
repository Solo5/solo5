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

/* This is the main header file for everything in the kernel */


/* common headers from the x86_64-elf gcc cross compiler */
#include <stdbool.h> 
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include "../loader/multiboot.h"
#include "../loader/loader_info.h"

/* alignment macros */
#define ALIGN_4K __attribute__((aligned(0x1000)))
#define ALIGN_64_BIT __attribute__((aligned(0x8)))

/* memory defines */
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define PAGE_MASK ~(0xfff)

/* convenient macro stringification */
#define STR_EXPAND(y) #y
#define STR(x) STR_EXPAND(x)

/* kernel.s: hang kernel, or hlt for interrupts */
void kernel_hang(void);
void kernel_wait(void);
void kernel_waitloop(void);
void kernel_busyloop(void);

/* interrupts.c: interrupt handling */
void interrupts_init(void);
void interrupts_enable(void);
void interrupts_disable(void);
void irq_mask(uint8_t irq);
void irq_clear(uint8_t irq);

void sse_enable(void);

/* mem.c: low-level page alloc routines */
void mem_init(struct multiboot_info *mb);
uint64_t mem_max_addr(void);
uint64_t read_cr3(void);

/* serial.c: console output for debugging */
void serial_init(void);
void serial_putc(char a);

/* time.c: clocksource */
void time_init(void);
uint64_t time_monotonic_ms(void);
uint64_t time_monotonic_ns(void);
uint64_t rdtsc(void);
uint64_t time_counts_since_startup(void);
void sleep(uint32_t ms);

/* printk.c: only has a few options: 
 *  %c   : character
 *  %s   : string
 *  %d   : int32_t in decimal
 *  %b   : uint8_t in hex
 *  %x   : uint16_t in hex
 *  %lx  : uint32_t in hex
 *  %llx : uint64_t in hex 
 */
void printk(char *fmt, ...);

/* ee_printf.c: a third-party printf slightly modified and with
 *              sprintf added 
 */
int printf(const char *fmt, ...);
int sprintf(char *str, const char *format, ...);

/* lib.c: expect this to grow... */
void memcheck(const void *ptr, uint8_t c, size_t size);
void *memset(void *ptr, uint8_t c, size_t size);
void *memcpy(void *dst, const void *src, size_t size);
void *memmove(void *dst, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
char *strcpy(char *dst, const char *src);
size_t strlen(const char *s);
int isxdigit(int c);
int abs(int j);
long int labs(long int j);
long long int llabs(long long int j);

/* pci.c: only enumerate for now */
void pci_enumerate(void);

/* virtio.c: mostly net for now */
void virtio_config_network(uint16_t base);
void virtio_config_block(uint16_t base);

extern uint8_t virtio_net_mac[];
uint8_t *virtio_net_pkt_get(int *size);  /* get a pointer to recv'd data */
void virtio_net_pkt_put(void);      /* we're done with recv'd data */
int virtio_net_xmit_packet(void *data, int len);

struct virtio_blk_req *virtio_blk_flush(uint64_t sector, void *data, int len);
struct virtio_blk_req *virtio_blk_write(uint64_t sector, void *data, int len);
struct virtio_blk_req *virtio_blk_read(uint64_t sector, int len);
int virtio_blk_write_sync(uint64_t sector, void *data, int len);
int virtio_blk_read_sync(uint64_t sector, void *data, int *len);
#define VIRTIO_BLK_SECTOR_SIZE    512
extern uint64_t virtio_blk_sectors;

void handle_virtio_interrupt(void);

/* net.c: ping for now */
void ping_serve(void);


/* accessing devices via port space */

static inline void outb(uint16_t port, uint8_t v){
    __asm__ __volatile__("outb %0,%1" : : "a" (v), "dN" (port));
}
static inline void outw(uint16_t port, uint16_t v){
    __asm__ __volatile__("outw %0,%1" : : "a" (v), "dN" (port));
}
static inline void outl(uint16_t port, uint32_t v){
    __asm__ __volatile__("outl %0,%1" : : "a" (v), "dN" (port));
}
static inline uint8_t inb(uint16_t port){
    uint8_t v;
    __asm__ __volatile__("inb %1,%0" : "=a" (v) : "dN" (port));
    return v;
}
static inline uint16_t inw(uint16_t port){
    uint16_t v;
    __asm__ __volatile__("inw %1,%0" : "=a" (v) : "dN" (port));
    return v;
}
static inline uint32_t inl(uint16_t port){
    uint32_t v;
    __asm__ __volatile__("inl %1,%0" : "=a" (v) : "dN" (port));
    return v;
}

static inline uint64_t inq(uint16_t port_lo){
    uint16_t port_hi = port_lo + 4;
    uint32_t lo, hi;
    
    __asm__ __volatile__("inl %1,%0" : "=a" (lo) : "dN" (port_lo));
    __asm__ __volatile__("inl %1,%0" : "=a" (hi) : "dN" (port_hi));

    return ((uint64_t)lo) | ((uint64_t)hi << 32);
}


#define PANIC(x...) do {                                   \
        printk("PANIC: %s:%d\n", __FILE__, __LINE__);      \
        printk(x);                                         \
        kernel_hang();                                     \
    } while(0)

#define assert(e) do {                              \
        if (!(e))                                   \
            PANIC("assertion failed: \"%s\"", #e);  \
    } while(0)

#define dprintk(x...) do {                      \
        if (dbg) {                              \
            printk(x);                          \
        }                                       \
    }while(0)

/* should only use outside of interrupt context */
#define atomic_printf(x...) do {                      \
        interrupts_disable();                         \
        printf(x);                                    \
        interrupts_enable();                          \
    }while(0)


/* dlmalloc configuration */
#undef WIN32
#define ABORT PANIC("aborting...")
#define MORECORE_CANNOT_TRIM
#define HAVE_MMAP 0
#define HAVE_MREMAP 0
#define MMAP_CLEARS 0
#define NO_MALLINFO 1
#define NO_MALLOC_STATS 1
#define LACKS_UNISTD_H
#define LACKS_FCNTL_H
#define LACKS_SYS_PARAM_H
#define LACKS_SYS_MMAN_H
#define LACKS_STRINGS_H
#define LACKS_STRING_H
#define LACKS_SYS_TYPES_H
#define LACKS_ERRNO_H
#define LACKS_STDLIB_H
#define LACKS_SCHED_H
#define LACKS_TIME_H
#define ABORT_ON_ASSERT_FAILURE 1
#define MALLOC_FAILURE_ACTION do { } while (0);

#define ENOMEM          12      /* Out of memory */
#define EINVAL          22      /* Invalid argument */

void *malloc(size_t bytes);
void  free(void *ptr);
void *sbrk(intptr_t increment);
void* memalign(size_t alignment, size_t bytes);


#include "app_undefined.h"
#include "app_stubs.h"

#endif

