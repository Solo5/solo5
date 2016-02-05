#include "kernel.h"
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
#define GDT_DESC_CODE_VAL (0x00af9a000000ffff)
/* granularity (23), big data seg (22), present (15), 
   type data rw (9), limit (16-19) */
#define GDT_DESC_DATA_VAL (0x00cf92000000ffff)

/* present (15), type: TSS available (8-11) limit (0-15) 0x67 */
#define GDT_DESC_TSS_LO_VAL(t)											\
	(((0x00008900														\
	   | ((uint64_t)(t) & 0xff000000)									\
	   | (((uint64_t)(t) >> 16) & 0xff)) << 32)							\
	 | (0x67 | ((uint64_t)(t) << 16)))

struct __attribute__((__packed__)) tss {
    uint32_t reserved;
    uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;
    uint64_t reserved2;
	uint64_t ist1;
	uint64_t ist2;
	uint64_t ist3;
	uint64_t ist4;
	uint64_t ist5;
	uint64_t ist6;
	uint64_t ist7;
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t iomap_base;
};

struct __attribute__((__packed__)) gdtptr {
    uint16_t limit;
    uint64_t base;
};


/* The actual memory for the TSS and GDT structures are here */
static struct tss tss ALIGN_4K;
static uint64_t gdt[GDT_NUM_ENTRIES] ALIGN_64_BIT;

/* initialize gdt, importantly putting in TSS entry */
void gdt_init(void) {
    struct gdtptr gdtptr;
    uint64_t _fixed_stack_top; /* defined in boot.s */
    _fixed_stack_top = 0x8000000; // XXX not really needed

    /* initialize GDT "pointer" */
    gdtptr.limit = sizeof(gdt) - 1;
    gdtptr.base = (uint64_t)&gdt;

    /* clear structures */
    memset(gdt, 0, sizeof(gdt));
    memset(&tss, 0, sizeof(struct tss));

    /* we will reuse the fixed bootstrap stack in boot.s for a known
       good interrupt stack.  At this point, we have already switched
       stacks to the top of memory. */
    tss.ist1 = tss.rsp0 = (uint64_t)&_fixed_stack_top;
    tss.iomap_base = sizeof(struct tss);

    gdt[GDT_DESC_CODE] = GDT_DESC_CODE_VAL;
    gdt[GDT_DESC_DATA] = GDT_DESC_DATA_VAL;
    gdt[GDT_DESC_TSS_LO] = GDT_DESC_TSS_LO_VAL(&tss);
    gdt[GDT_DESC_TSS_HI] = 0;

	gdt_load((uint64_t)&gdtptr);
	tss_load((uint16_t)GDT_DESC_OFFSET(GDT_DESC_TSS));
}
