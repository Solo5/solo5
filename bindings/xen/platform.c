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
 * Low 1MB is unused as on other targets, we place page tables in this space.
 */
#define PLATFORM_MEM_START 0x100000UL
#define PLATFORM_PAGETABLE_START 0x10000UL

/*
 * Maximum guest-physical memory size is 4GB.
 */
#define PLATFORM_MAX_MEM_SIZE 0x100000000UL

static uint64_t mem_size;

struct __attribute__((packed)) e820_map_entry {
    uint64_t addr;
    uint64_t size;
    uint32_t type;
};

#define E820_MAP_TYPE_RAM 1

#define L1_REGION_SIZE (1UL << 12)
#define L2_REGION_SIZE (1UL << 21)
#define L3_REGION_SIZE (1UL << 30)

/*
 * Private ABI for Mirage/Xen to determine the guest-virtual address range
 * usable for importing grant mappings from other Xen domains.
 */
void solo5__xen_get_gntmap_area(uint64_t *addr, size_t *size)
{
    assert(addr);
    assert(size);
    *addr = PLATFORM_MAX_MEM_SIZE;
    *size = L3_REGION_SIZE;
}

static inline uint64_t align_up(uint64_t val, uint64_t align)
{
    return (val + (align - 1)) & -align;
}

static void pagetable_init(void)
{
    extern char _stext[], _etext[], _erodata[];

    uint64_t paddr;
    /*
     * Highest mapped guest-physical address. PLATFORM_MAX_MEM_SIZE (currently
     * 4GB) is the limit we support for use by the guest; we map an additional
     * 1GB region past that for use by Mirage/Xen for importing grant mappings
     * from other Xen domains.
     *
     * Note that Xen maps various structures just below 4GB in the
     * guest-physical address space.
     */
    uint64_t paddr_end = PLATFORM_MAX_MEM_SIZE + L3_REGION_SIZE;

    /*
     * L1 (PTEs): Addresses up to (paddr_l1_end - 1) are mapped using 4kb
     * pages.
     */
    uint64_t paddr_l1_end = align_up((uint64_t)&_erodata, L2_REGION_SIZE);
    uint64_t *e = (uint64_t *)PLATFORM_PAGETABLE_START;
    uint64_t l1e_addr = (uint64_t)e;
    for (paddr = 0; paddr < paddr_l1_end; paddr += L1_REGION_SIZE) {
        if (paddr < (uint64_t)&_stext)
            /*
             * Addresses below .text are unmapped, this includes
             * these page tables.
             */
            *e = 0;
        else if (paddr < (uint64_t)&_etext)
            /*
             * .text is mapped as read-only and executable.
             */
            *e = paddr | X86_PTE_P;
        else if (paddr < (uint64_t)&_erodata)
            /*
             * .rodata is mapped as read-only and non-executable.
             */
            *e = paddr | X86_PTE_P | X86_PTE_XD;
        else
            /*
             * Everything above that is mapped read-write and non-executable.
             */
            *e = paddr | X86_PTE_P | X86_PTE_W | X86_PTE_XD;

        e += 1;
    }

    /*
     * L2 (PDEs).
     */
    uint64_t l2e_addr = (uint64_t)e;
    for (paddr = 0; paddr < paddr_end; paddr += L2_REGION_SIZE) {
        if (paddr < paddr_l1_end) {
            /*
             * References L1, must be marked RWX at this level.
             */
            *e = l1e_addr | X86_PTE_P | X86_PTE_W;
            l1e_addr += PAGE_SIZE;
        }
        else {
            *e = paddr | X86_PTE_P | X86_PTE_W | X86_PTE_PS | X86_PTE_XD;
        }

        e += 1;
    }

    /*
     * L3 (PDPTEs).
     */
    uint64_t l3e_addr = (uint64_t)e;
    uint64_t l4e_addr = l3e_addr + PAGE_SIZE;
    /*
     * L3 and L4 do not fill a whole page, zero-fill these first.
     */
    memset(e, 0, 2 * PAGE_SIZE);
    for (paddr = 0; paddr < paddr_end; paddr += L3_REGION_SIZE) {
        /*
         * References L2, must be marked RWX at this level.
         */
        *e = l2e_addr | X86_PTE_P | X86_PTE_W;
        l2e_addr += PAGE_SIZE;

        e += 1;
    }

    /*
     * Single L4 (PML4). References L3, must be marked RWX at this level.
     */
    e = (uint64_t *)l4e_addr;
    *e = l3e_addr | X86_PTE_P | X86_PTE_W;

    /*
     * Switch to new page tables.
     */
    __asm__ __volatile__(
            "mov %0, %%cr3"
            : : "a" (l4e_addr)
            : "memory"
    );
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
    pagetable_init();
    hypercall_init();
    console_init();

    /*
     * The Xen hvm_start_info may be anywhere in memory, so take a copy of
     * the command line before we initialise memory allocation.
     */
    const struct hvm_start_info *si = (struct hvm_start_info *)arg;
    assert(si->magic == XEN_HVM_START_MAGIC_VALUE);

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
     * Detect physical memory. On PVH on x86, memory will start at 0 and
     * may or may not be followed by a legacy memory hole at 640kB. Since we
     * don't make any significant use of the low memory, we can simplify by
     * just looking for the first chunk of memory that ends at >= 1mB
     * (PLATFORM_MEM_START).
     */
    if (si->version >= 1 && si->memmap_entries > 0) {
        assert(si->memmap_paddr);
        struct hvm_memmap_table_entry *m =
            (struct hvm_memmap_table_entry *)si->memmap_paddr;

        for (unsigned i = 0; i < si->memmap_entries; i++) {
            if (m[i].type == XEN_HVM_MEMMAP_TYPE_RAM &&
                    m[i].addr + m[i].size >= PLATFORM_MEM_START) {
                mem_size = (m[i].addr + m[i].size) & PAGE_MASK;
                break;
            }
        }
    }
    /*
     * No memory map in Xen hvm_start_info; try and get an E820-style memory
     * map from Xen instead.
     */
    else {
        struct e820_map_entry e820_map[32];
        xen_memory_map_t xmm;
        xmm.nr_entries = 32;
        set_xen_guest_handle(xmm.buffer, e820_map);
        int rc = hypercall__memory_op(XENMEM_memory_map, &xmm);
        assert(rc == 0);
        assert(xmm.nr_entries > 0);

        for (unsigned i = 0; i < xmm.nr_entries; i++) {
            if (e820_map[i].type == E820_MAP_TYPE_RAM &&
                    e820_map[i].addr + e820_map[i].size >= PLATFORM_MEM_START) {
                mem_size = (e820_map[i].addr + e820_map[i].size) & PAGE_MASK;
                break;
            }
        }
    }
    assert(mem_size);

    /*
     * Cap our usable memory size to PLATFORM_MAX_MEM_SIZE which is the highest
     * address we define page tables for.
     */
    if (mem_size > PLATFORM_MAX_MEM_SIZE)
        mem_size = PLATFORM_MAX_MEM_SIZE;

    /*
     * Map the hypervisor shared_info page.
     */
    int rc = hypercall_physmap_add_shared_info(0,
            (uintptr_t)&HYPERVISOR_SHARED_INFO >> PAGE_SHIFT);
    assert(rc == 0);

    /*
     * Initialise APIC.
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
