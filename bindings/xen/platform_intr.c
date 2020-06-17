/*
 * Copyright (c) 2015-2020 Contributors as noted in the AUTHORS file
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

#include "bindings.h"

#if !defined(__x86_64__)
#error Not implemented
#endif

#define MSR_IA32_APIC_BASE        0x1b
#define MSR_IA32_APIC_BASE_BSP    (1UL << 8)
#define MSR_IA32_APIC_BASE_ENABLE (1UL << 11)
#define MSR_IA32_APIC_BASE_MASK   (0xffffffUL << 12)
#define APIC_BASE                 0xfee00000UL

#define DECLARE_MMIO32_REG(name, addr) \
    static uint32_t * const name = (uint32_t *)(addr)

DECLARE_MMIO32_REG(APIC_VERSION, APIC_BASE + 0x30);
DECLARE_MMIO32_REG(APIC_SVR,     APIC_BASE + 0xf0);
#define APIC_SVR_ENABLE          (1U << 8)
DECLARE_MMIO32_REG(APIC_EOI,     APIC_BASE + 0xb0);

static inline uint32_t mmio_readl(const uint32_t *addr)
{
    return __atomic_load_n(addr, __ATOMIC_SEQ_CST);
}

static inline void mmio_writel(uint32_t *addr, uint32_t val)
{
    __atomic_store_n(addr, val, __ATOMIC_SEQ_CST);
}

static inline uint64_t rdmsrq(uint32_t base)
{
    uint32_t low, high;

    __asm__ __volatile__(
            "rdmsr" :
            "=a" (low),
            "=d" (high) :
            "c" (base)
    );
    return ((uint64_t)high << 32) | low;
}

static inline void wrmsrq(uint32_t base, uint64_t val)
{
    __asm__ __volatile__(
            "wrmsr" ::
            "c" (base),
            "a" ((uint32_t)val),
            "d" ((uint32_t)(val >> 32))
    );
}

static int spurious_vector_handler(void *arg __attribute__((unused)))
{
    return 1;
}

void platform_intr_init(void)
{
    /*
     * Globally enable the APIC and verify that APIC_BASE is set to the
     * architecturally-defined value. This will always hold for Xen, and we do
     * not support relocating APIC_BASE to a non-default value.
     *
     * N.B.: Technically we should also check CPUID for the presence of an
     * APIC, but we can assume that we won't be running on a CPU/hypervisor
     * that is that old.
     */
    uint64_t apic_base = rdmsrq(MSR_IA32_APIC_BASE);
    wrmsrq(MSR_IA32_APIC_BASE, apic_base | MSR_IA32_APIC_BASE_ENABLE);
    apic_base = rdmsrq(MSR_IA32_APIC_BASE);
    if (!(apic_base & MSR_IA32_APIC_BASE_ENABLE) ||
            ((apic_base & MSR_IA32_APIC_BASE_MASK) != APIC_BASE)) {
        log(ERROR, "Solo5: Could not enable APIC or not present\n");
        assert(false);
    }
    /*
     * Additional sanity checks: The CPU should be a BSP and something sane
     * should show in the APIC_VERSION register.
     */
    assert(apic_base & MSR_IA32_APIC_BASE_BSP);
    uint32_t apic_version = mmio_readl(APIC_VERSION) & 0xff;
    assert(apic_version);

    /*
     * Locally enable the APIC, and set the spurious interrupt vector to
     * IRQ 15 (vector #47), since that is the highest vector we have support
     * in our IDT for.
     */
    intr_register_irq(15, spurious_vector_handler, NULL);
    uint32_t apic_svr = mmio_readl(APIC_SVR);
    apic_svr |= (32 + 15) | APIC_SVR_ENABLE;
    mmio_writel(APIC_SVR, apic_svr);
}

void platform_intr_ack_irq(unsigned irq __attribute__((unused)))
{
    /*
     * Intel SDM doesn't say what to write here, so presumably any write
     * is fine.
     */
    mmio_writel(APIC_EOI, 1);
}

/*
 * We only use the APIC for upcall delivery from Xen, which is performed via
 * a self-targeted IPI, so no need to implement mask or clear here.
 */
void platform_intr_mask_irq(unsigned irq __attribute__((unused)))
{
}

void platform_intr_clear_irq(unsigned irq __attribute__((unused)))
{
}
