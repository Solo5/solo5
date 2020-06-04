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
#include "hypercall.h"

#include "xen/arch-x86/cpuid.h"
#include "xen/arch-x86/hvm/start_info.h"
#include "xen/hvm/params.h"

/*
 * Xen shared_info page is mapped here.
 *
 * Note the explicit initialisation to { 0 }, this is necessary to prevent
 * the compiler from declaring this with .comm, which breaks the aliasing
 * below.
 */
uint8_t HYPERVISOR_SHARED_INFO[PAGE_SIZE]
__attribute__((aligned(PAGE_SIZE))) = { 0 };

/*
 * Private ABI to allow direct access to the shared_info structures
 * by Mirage/Xen. May change or go away without warning.
 */
extern uint8_t *solo5__xen_HYPERVISOR_SHARED_INFO
__attribute__((alias("HYPERVISOR_SHARED_INFO")));

/*
 * Declared in hypercall_page.S, needed for hypercall_init().
 */
extern const uint8_t HYPERCALL_PAGE[PAGE_SIZE];

static char cmdline[8192];

/*
 * Low 1MB is unused as on other targets.
 */
#define PLATFORM_MEM_START 0x100000

/*
 * Maximum memory size is 4GB, as mapped in pagetable.S.
 */
#define PLATFORM_MAX_MEM_SIZE 0x100000000

static uint64_t mem_size;

static inline void
x86_cpuid(uint32_t level, uint32_t *eax_out, uint32_t *ebx_out,
        uint32_t *ecx_out, uint32_t *edx_out)
{
    uint32_t eax_, ebx_, ecx_, edx_;

    __asm__(
        "cpuid"
        : "=a" (eax_), "=b" (ebx_), "=c" (ecx_), "=d" (edx_)
        : "0" (level)
    );
    *eax_out = eax_;
    *ebx_out = ebx_;
    *ecx_out = ecx_;
    *edx_out = edx_;
}

static void hypercall_init(void)
{
    uint32_t base, eax, ebx, ecx, edx;
    bool found = false;

    for (base = XEN_CPUID_FIRST_LEAF; base < XEN_CPUID_FIRST_LEAF + 0x100000;
            base += 0x100) {
        x86_cpuid(base, &eax, &ebx, &ecx, &edx);
        if ((ebx == XEN_CPUID_SIGNATURE_EBX) &&
                (ecx == XEN_CPUID_SIGNATURE_ECX) &&
                (edx == XEN_CPUID_SIGNATURE_EDX) &&
                ((eax - base) >= 2)) {
            found = true;
            break;
        }
    }
    assert(found);

    x86_cpuid(base + 2, &eax, &ebx, &ecx, &edx);
    __asm__ __volatile("wrmsr" ::
        "c" (base),
        "a" ((uint32_t)((uintptr_t)&HYPERCALL_PAGE)),
        "d" ((uint32_t)((uintptr_t)&HYPERCALL_PAGE >> 32))
    );
    cc_barrier();

    /*
     * Confirm that the RET poison we initialise the hypercall page with
     * has been overwritten, i.e. the page is correctly mapped.
     */
    assert(HYPERCALL_PAGE[0] != 0xc3);
}


void platform_init(const void *arg __attribute__((unused)))
{
    hypercall_init();
    console_init();

    /*
     * The Xen hvm_start_info may be anywhere in memory, so take a copy of
     * the command line before we initialise memory allocation.
     */
    const struct hvm_start_info *si = (struct hvm_start_info *)arg;
    assert(si->magic == XEN_HVM_START_MAGIC_VALUE);
    assert(si->version >= 1);

    if (si->cmdline_paddr) {
        char *hvm_cmdline = (char *)si->cmdline_paddr;
        size_t cmdline_len = strlen(hvm_cmdline);

        if (cmdline_len >= sizeof(cmdline)) {
            cmdline_len = sizeof(cmdline) - 1;
            log(WARN, "Solo5: warning: command line too long, truncated\n");
        }
        memcpy(cmdline, hvm_cmdline, cmdline_len);
    }
    else {
        cmdline[0] = 0;
    }

    /*
     * Look for the first chunk of memory at PLATFORM_MEM_START.
     */
    assert(si->memmap_paddr);
    struct hvm_memmap_table_entry *m =
        (struct hvm_memmap_table_entry *)si->memmap_paddr;
    uint32_t i;

    for (i = 0; i < si->memmap_entries; i++) {
        if (m[i].type == XEN_HVM_MEMMAP_TYPE_RAM &&
                m[i].addr + m[i].size >= PLATFORM_MEM_START) {
            mem_size = (m[i].addr + m[i].size) & PAGE_MASK;
            break;
        }
    }
    assert(mem_size);

    /*
     * Cap our memory size to PLATFORM_MAX_MEM_SIZE which boot.S defines page
     * tables for.
     */
    if (mem_size > PLATFORM_MAX_MEM_SIZE)
        mem_size = PLATFORM_MAX_MEM_SIZE;

    /*
     * Map the hypervisor shared_info page.
     */
    int rc;
    rc = hypercall_physmap_add_shared_info(0,
            (uintptr_t)&HYPERVISOR_SHARED_INFO >> PAGE_SHIFT);
    assert(rc == 0);

    /*
     * Initialise PIC. XXX: Is this actually necessary on Xen PVH?
     */
    platform_intr_init();
}

const char *platform_cmdline(void)
{
    return cmdline;
}

uint64_t platform_mem_size(void)
{
    return mem_size;
}

void platform_exit(int status, void *cookie __attribute__((unused)))
{
    int reason;

    if (status == SOLO5_EXIT_ABORT)
        reason = SHUTDOWN_crash;
    else
        reason = SHUTDOWN_poweroff;
    hypercall_shutdown(reason);

    /*
     * If we got here, we are aborting in early boot or hypercalls are failing
     * for some reason, so just halt.
     */
    platform_puts("Solo5: Halted\n", 14);
    cpu_halt();
}

int platform_puts(const char *buf, int n)
{
    console_write(buf, n);
    return n;
}

void solo5_console_write(const char *buf, size_t size)
{
    (void)platform_puts(buf, size);
}

int platform_set_tls_base(uint64_t base)
{
    cpu_set_tls_base(base);
    return 0;
}
