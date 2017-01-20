#ifndef __UKVM_KERNEL_H__
#define __UKVM_KERNEL_H__

#include "../kernel.h"
#include "ukvm.h"

/* gdt.c: initialize segment descriptors */
/* TODO: remove these, as they should be already done in ukvm */
void gdt_init(void);
void gdt_load(uint64_t gdtptr);
void tss_load(uint16_t tss);

void mem_init(uint64_t size, uint64_t _kernel_end);

int ukvmclock_init(void);
uint64_t ukvmclock_monotonic(void);
uint64_t ukvmclock_epochoffset(void);

#endif
