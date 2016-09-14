#include "kernel.h"

/* granularity (23), long mode (21), present (15),
 * always 1 (12, 11), readable (9), limit (16-19)
 */
#define GDT_DESC_CODE_VAL (0x00af9a000000ffff)
/* granularity (23), big data seg (22), present (15),
 * type data rw (9), limit (16-19)
 */
#define GDT_DESC_DATA_VAL (0x00cf92000000ffff)

struct __attribute__((__packed__)) gdtptr {
    uint16_t limit;
    uint64_t base;
};

uint64_t cpu_gdt64[GDT_NUM_ENTRIES] ALIGN_64_BIT;

/*
 * Ukvm starts up with a bootstrap GDT which is "invisible" to the guest, init
 * and switch to our own GDT.
 */
void gdt_init(void)
{
    struct gdtptr gdtptr;

    /* initialize GDT "pointer" */
    gdtptr.limit = sizeof(cpu_gdt64) - 1;
    gdtptr.base = (uint64_t)&cpu_gdt64;

    /* clear structures */
    memset(cpu_gdt64, 0, sizeof(cpu_gdt64));

    cpu_gdt64[GDT_DESC_CODE] = GDT_DESC_CODE_VAL;
    cpu_gdt64[GDT_DESC_DATA] = GDT_DESC_DATA_VAL;

    cpu_gdt_load((uint64_t)&gdtptr);
}
