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

#include "loader.h"
/* The GDT contains "segment descriptors".  The first must be NULL.
 * The code and data are 64 bits long.  They basically start at 0 and
 * have a maximum limit, but I believe the limit is ignored in long
 * mode.  The TSS segment descriptor is 128 bits long.  It points to a
 * specific structure in memory which specifies what stack to use when
 * an interrupt arrives.  Encoding the address is a bit messy.
 *
 * The TSS is a legacy task management structure that is used in
 * x86_64 long mode as a mechanism to specify an interrupt stack
 * (e.g., a "known good" stack) to process interrupts with.  There
 * must be at least one TSS.  
 *
 * For 64-bit execution, we want to have a NULL SS (segments aren't
 * used anyway and seem to confuse the CPU on returning from
 * interrupts).  To do this we change the SS value on the stack before
 * returning from the first interrupt at setup time.  It's a bit of a
 * nasty hack, called the "SS hack" below.
 */

/* granularity (23), long mode (21), present (15), 
   always 1 (12, 11), readable (9), limit (16-19) */
#define GDT_DESC_CODE_FLAGS_HI (0x00af9a00)
/* granularity (23), present (15), 
   always 1 (12), writable (9), limit (16-19) */
#define GDT_DESC_DATA_FLAGS_HI (0x008f9a00)

/* segment limit */
#define GDT_DESC_DATA_FLAGS_LO (0xffff)
#define GDT_DESC_CODE_FLAGS_LO (0xffff)

/* present (15), type: TSS available (8-11) limit (0-15) 0x67 */
#define GDT_DESC_TSS_FLAGS_HI(t) (0x00008900                            \
                                  | ((uint32_t)(t) & 0xff000000)        \
                                  | (((uint32_t)(t) >> 16) & 0xff))
#define GDT_DESC_TSS_FLAGS_LO(t) (0x67                                  \
                                  | ((uint32_t)(t) << 16))

struct __attribute__((__packed__)) qw {
    uint32_t lo1;
    uint32_t hi1;
    uint32_t lo2;
    uint32_t hi2;
};

struct __attribute__((__packed__)) tss {
    uint32_t reserved;
    uint32_t rsp0_lo;
    uint32_t rsp0_hi;
    uint32_t rsp1_lo;
    uint32_t rsp1_hi;
    uint32_t rsp2_lo;
    uint32_t rsp2_hi;
    uint32_t reserved2;
    uint32_t reserved3;
    uint32_t ist1_lo;
    uint32_t ist1_hi;
    uint32_t ist2_lo;
    uint32_t ist2_hi;
    uint32_t ist3_lo;
    uint32_t ist3_hi;
    uint32_t ist4_lo;
    uint32_t ist4_hi;
    uint32_t ist5_lo;
    uint32_t ist5_hi;
    uint32_t ist6_lo;
    uint32_t ist6_hi;
    uint32_t ist7_lo;
    uint32_t ist7_hi;
    uint32_t reserved4;
    uint32_t reserved5;
    uint16_t reserved6;
    uint16_t iomap_base;
};

struct __attribute__((__packed__)) gdtptr {
    uint16_t limit;
    uint64_t base;
};

/* assembly functions to load GDT, TSS */
extern void to64_prep_gdt(struct gdtptr *ptr);
extern void to64_prep_tss(uint16_t off);

/* The actual memory for the TSS and GDT structures are here */
static struct tss tss ALIGN_4K;
static struct dw gdt[GDT_NUM_ENTRIES] ALIGN_64_BIT;

/* initialize gdt, importantly putting in TSS entry */
void gdt_init(void) {
    struct gdtptr gdtptr;
    extern uint32_t stack_top; /* bootstrap stack in boot.s */

    /* initialize GDT "pointer" */
    gdtptr.limit = sizeof(gdt) - 1;
    gdtptr.base = (uint32_t) &gdt;

    /* clear structures */
    memset(gdt, 0, gdtptr.limit);
    memset(&tss, 0, sizeof(struct tss));

    /* we will reuse the bootstrap stack in the loader for a known
       good interrupt stack.  We switch stacks to the top of memory
       when we jump to 64 bit long mode. */
    tss.ist1_lo = tss.rsp0_lo = (uint32_t)&stack_top;
    tss.ist1_hi = tss.rsp0_hi = 0;
    tss.iomap_base = sizeof(struct tss);

    gdt[GDT_DESC_CODE].hi = GDT_DESC_CODE_FLAGS_HI;
    gdt[GDT_DESC_CODE].lo = GDT_DESC_CODE_FLAGS_LO;
    gdt[GDT_DESC_DATA].hi = GDT_DESC_DATA_FLAGS_HI;
    gdt[GDT_DESC_DATA].lo = GDT_DESC_DATA_FLAGS_LO;
    gdt[GDT_DESC_TSS_LO].hi = GDT_DESC_TSS_FLAGS_HI(&tss);
    gdt[GDT_DESC_TSS_LO].lo = GDT_DESC_TSS_FLAGS_LO(&tss);
    gdt[GDT_DESC_TSS_HI].hi = 0;
    gdt[GDT_DESC_TSS_HI].lo = 0;

    to64_prep_gdt(&gdtptr);
    to64_prep_tss((uint16_t)GDT_DESC_OFFSET(GDT_DESC_TSS));
}
