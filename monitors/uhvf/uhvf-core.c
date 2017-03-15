/* Copyright (c) 2016, IBM
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

/* We used several existing projects as guides
 *   hvdos: https://github.com/mist64/hvdos
 *   xhyve: https://github.com/mist64/xhyve
 *   ukvm: https://github.com/solo5/solo5
 */
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <libgen.h> /* for `basename` */
#include <inttypes.h>
#include <signal.h>
#include <limits.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/select.h>
#include <Hypervisor/hv.h>
#include <Hypervisor/hv_vmx.h>

#include "../ukvm-private.h"
#include "../ukvm-cpu.h"
#include "../ukvm.h"
#include "../unikernel-monitor.h"

#include <mach-o/loader.h>
#include <mach/machine/thread_status.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

static struct platform platform;
static clock_serv_t cclock;

/*
 * Memory map:
 *
 * 0x100000    loaded elf file (linker script dictates location)
 * ########    unused
 * 0x013000
 * 0x012000    bootstrap pde
 * 0x011000    bootstrap pdpte
 * 0x010000    bootstrap pml4
 * ########    command line arguments
 * 0x002000    ukvm_boot_info
 * 0x001000    bootstrap gdt (contains correct code/data/ but tss points to 0)
 */

#define GUEST_PAGE_SIZE 0x200000   /* 2 MB pages in guest */

#define BOOT_GDT     0x1000
#define BOOT_INFO    0x2000
#define BOOT_PML4    0x10000
#define BOOT_PDPTE   0x11000
#define BOOT_PDE     0x12000

/* read GPR */
uint64_t rreg(hv_vcpuid_t vcpu, hv_x86_reg_t reg)
{
    uint64_t v;

    if (hv_vcpu_read_register(vcpu, reg, &v))
        abort();

    return v;
}

/* write GPR */
void wreg(hv_vcpuid_t vcpu, hv_x86_reg_t reg, uint64_t v)
{
    if (hv_vcpu_write_register(vcpu, reg, v))
        abort();
}

/* read VMCS field */
static uint64_t rvmcs(hv_vcpuid_t vcpu, uint32_t field)
{
    uint64_t v;

    if (hv_vmx_vcpu_read_vmcs(vcpu, field, &v))
        abort();

    return v;
}

/* write VMCS field */
static void wvmcs(hv_vcpuid_t vcpu, uint32_t field, uint64_t v)
{
    if (hv_vmx_vcpu_write_vmcs(vcpu, field, v))
        abort();
}

/* desired control word constrained by hardware/hypervisor capabilities */
static uint64_t cap2ctrl(uint64_t cap, uint64_t ctrl)
{
    return (ctrl | (cap & 0xffffffff)) & (cap >> 32);
}


void platform_load_code(struct platform *p, const char *file, /* IN */
                        uint64_t *p_entry, uint64_t *p_end)   /* OUT */
{
    int fd_kernel;
    uint32_t off;
    int i;
    uint8_t *macho;
    struct stat fd_stat;
    struct mach_header_64 *hdr;
    int dbg = 0;
    
    fd_kernel = open(file, O_RDONLY);
    if (fd_kernel == -1)
        goto out_error;
    fstat(fd_kernel, &fd_stat);
    
    macho = mmap(NULL, fd_stat.st_size, PROT_READ, MAP_SHARED, fd_kernel, 0);
    if (macho == MAP_FAILED)
        goto out_error;

    hdr = (struct mach_header_64 *)macho;

    if (hdr->magic != MH_MAGIC_64
        || hdr->cputype != CPU_TYPE_X86_64)
        goto out_invalid;

    off = sizeof(struct mach_header_64);
    if (dbg) printf("%d load commands\n", hdr->ncmds);
    
    for (i = 0; i < hdr->ncmds; i++) {
        struct load_command *lc = (struct load_command *)(macho + off);
        
        if (dbg) printf("0x%08x ", off);
        switch (lc->cmd) {
        case LC_UNIXTHREAD: {
            struct x86_thread_state *ts;
            ts = (struct x86_thread_state *)(macho + off
                                             + sizeof(struct load_command));

            if (dbg) printf("LC_UNIXTHREAD [%d]\n", lc->cmdsize);
            assert(ts->tsh.flavor == x86_THREAD_STATE64);

            *p_entry = ts->uts.ts64.__rip;

            if (dbg) printf("    entry point is 0x%llx\n", *p_entry);
            break;
        }
        case LC_UUID:
            if (dbg) printf("LC_UUID\n");
            break;
        case LC_SOURCE_VERSION:
            if (dbg) printf("LC_SOURCE_VERSION\n");
            break;
        case LC_SYMTAB:
            if (dbg) printf("LC_SYMTAB\n");
            break;
        case LC_SEGMENT_64: {
            struct segment_command_64 *sc;
            int sects;

            sc = (struct segment_command_64 *)(macho + off);
            if (dbg)
                printf("LC_SEGMENT_64 [%08llx - %08llx] %s (%d sections)\n",
                       sc->vmaddr, sc->vmaddr + sc->vmsize,
                       sc->segname, sc->nsects);

            for (sects = 0; sects < sc->nsects; sects++) {
                struct section_64 *s = (struct section_64 *)(macho + off
                                        + sizeof(struct segment_command_64)
                                        + sects * sizeof(struct section_64));

                if (dbg) printf("    [%08llx - %08llx] (0x%x) %s:%s\n",
                                s->addr, s->addr + s->size, s->flags,
                                s->segname, s->sectname);

                if ((s->flags & 0x7) == S_ZEROFILL) {
                    if (dbg) printf("zeroing %lld bytes at 0x%llx\n",
                                    s->size, s->addr);
                    memset(p->mem + s->addr, 0, s->size);
                } else {
                    if (dbg) printf("copying %lld bytes from 0x%x to 0x%llx\n",
                                    s->size, s->offset, s->addr);
                    memcpy(p->mem + s->addr, macho + s->offset, s->size);
                }
            }

            *p_end = sc->vmaddr + sc->vmsize;
            break;
        }
        default:
            printf("unknown %x (%d)\n", lc->cmd, lc->cmd);
        }
            
        off += lc->cmdsize;
    }
    
    return;

 out_error:
    err(1, "%s", file);
out_invalid:
    errx(1, "%s: Exec format error", file);
}

void platform_setup_system_64bit(struct platform *p, uint64_t cr0,
                                 uint64_t cr4, uint64_t efer)
{
    wvmcs(p->vcpu, VMCS_GUEST_CR0, cr0);
    wvmcs(p->vcpu, VMCS_GUEST_CR4, cr4);
    wvmcs(p->vcpu, VMCS_GUEST_IA32_EFER, efer);
}

void platform_setup_system_page_tables(struct platform *p, uint64_t pml4)
{
    wvmcs(p->vcpu, VMCS_GUEST_CR3, pml4);
}

void platform_setup_system_gdt(struct platform *p,
                               uint64_t cs_idx,
                               uint64_t ds_idx,
                               uint64_t off,
                               uint64_t limit)
{
    uint64_t *gdt_entry;
    gdt_entry = ((uint64_t *) (p->mem + off));
    uint64_t cs_off = cs_idx * sizeof(uint64_t);
    uint64_t ds_off = ds_idx * sizeof(uint64_t);
    uint64_t cs_ar = (gdt_entry[cs_idx] & 0x0f0ff0000000000) >> 40;
    uint64_t ds_ar = (gdt_entry[ds_idx] & 0x0f0ff0000000000) >> 40;

    wvmcs(p->vcpu, VMCS_GUEST_CS_BASE, 0);
    wvmcs(p->vcpu, VMCS_GUEST_CS_LIMIT, 0xffffffff);
    wvmcs(p->vcpu, VMCS_GUEST_CS_AR, cs_ar);
    wvmcs(p->vcpu, VMCS_GUEST_SS_BASE, 0);
    wvmcs(p->vcpu, VMCS_GUEST_SS_LIMIT, 0xffffffff);
    wvmcs(p->vcpu, VMCS_GUEST_SS_AR, ds_ar);
    wvmcs(p->vcpu, VMCS_GUEST_DS_BASE, 0);
    wvmcs(p->vcpu, VMCS_GUEST_DS_LIMIT, 0xffffffff);
    wvmcs(p->vcpu, VMCS_GUEST_DS_AR, ds_ar);
    wvmcs(p->vcpu, VMCS_GUEST_ES_BASE, 0);
    wvmcs(p->vcpu, VMCS_GUEST_ES_LIMIT, 0xffffffff);
    wvmcs(p->vcpu, VMCS_GUEST_ES_AR, ds_ar);
    wvmcs(p->vcpu, VMCS_GUEST_FS_BASE, 0);
    wvmcs(p->vcpu, VMCS_GUEST_FS_LIMIT, 0xffffffff);
    wvmcs(p->vcpu, VMCS_GUEST_FS_AR, ds_ar);
    wvmcs(p->vcpu, VMCS_GUEST_GS_BASE, 0);
    wvmcs(p->vcpu, VMCS_GUEST_GS_LIMIT, 0xffffffff);
    wvmcs(p->vcpu, VMCS_GUEST_GS_AR, ds_ar);

    wvmcs(p->vcpu, VMCS_GUEST_CS, cs_off);
    wvmcs(p->vcpu, VMCS_GUEST_DS, ds_off);
    wvmcs(p->vcpu, VMCS_GUEST_SS, ds_off);
    wvmcs(p->vcpu, VMCS_GUEST_ES, ds_off);
    wvmcs(p->vcpu, VMCS_GUEST_FS, ds_off);
    wvmcs(p->vcpu, VMCS_GUEST_GS, ds_off);

    wvmcs(p->vcpu, VMCS_GUEST_GDTR_BASE, off);
    wvmcs(p->vcpu, VMCS_GUEST_GDTR_LIMIT, limit);

    /* no IDT: all interrupts/exceptions exit */
    wvmcs(p->vcpu, VMCS_GUEST_IDTR_BASE, 0);
    wvmcs(p->vcpu, VMCS_GUEST_IDTR_LIMIT, 0);

    wvmcs(p->vcpu, VMCS_GUEST_TR_BASE, 0);
    wvmcs(p->vcpu, VMCS_GUEST_TR_LIMIT, 0);
    wvmcs(p->vcpu, VMCS_GUEST_TR_AR, 0x0000008b);
    wvmcs(p->vcpu, VMCS_GUEST_LDTR_BASE, 0);
    wvmcs(p->vcpu, VMCS_GUEST_LDTR_LIMIT, 0xffff);
    wvmcs(p->vcpu, VMCS_GUEST_LDTR_AR, 0x00000082);
}

void platform_setup_system(struct platform *p, uint64_t entry,
                           uint64_t boot_info)
{
    wvmcs(p->vcpu, VMCS_GUEST_RFLAGS, 0x2);
    wvmcs(p->vcpu, VMCS_GUEST_RIP, entry);
    wvmcs(p->vcpu, VMCS_GUEST_RSP, GUEST_SIZE - 8);
    wreg(p->vcpu, HV_X86_RDI, boot_info);

    /* trap everything for cr0 and cr4 */
    wvmcs(p->vcpu, VMCS_CTRL_CR0_MASK, 0xffffffff);
    wvmcs(p->vcpu, VMCS_CTRL_CR4_MASK, 0xffffffff);
    wvmcs(p->vcpu, VMCS_CTRL_CR0_SHADOW, rvmcs(p->vcpu, VMCS_GUEST_CR0));
    wvmcs(p->vcpu, VMCS_CTRL_CR4_SHADOW, rvmcs(p->vcpu, VMCS_GUEST_CR4));
}

#define VMX_CTRLS(v,c,t,f) do {                 \
    uint64_t cap;                               \
    if (hv_vmx_read_capability((c), &cap)) {    \
        abort();                                \
    }                                           \
                                                \
    uint64_t zeros = cap & 0xffffffff;              \
    uint64_t ones = (cap >> 32) & 0xffffffff;       \
    uint64_t setting = cap2ctrl(cap, (f));          \
    if (0) {                                        \
        printf("%s %s\n", #c, #t);                  \
        printf("   0s:      0x%08llx\n", zeros);    \
        printf("   1s:      0x%08llx\n", ones);     \
        printf("   setting: 0x%08llx\n", setting);  \
    }                                               \
    wvmcs((v), (t), setting);                       \
    } while (0)                                     \

int platform_init(struct platform **pdata_p)
{
    hv_vcpuid_t vcpu;
    uint8_t *mem;

    /* create a VM instance for the current task */
    if (hv_vm_create(HV_VM_DEFAULT))
        abort();

    /* allocate some guest physical memory */
    mem = (uint8_t *)valloc(GUEST_SIZE);
    if (!mem)
        abort();

    memset(mem, 0, GUEST_SIZE);

    /* map a segment of guest physical memory into the guest physical
     * address space of the vm (at address 0)
     */
    if (hv_vm_map(mem, 0, GUEST_SIZE,
                  HV_MEMORY_READ | HV_MEMORY_WRITE | HV_MEMORY_EXEC))
        abort();

    /* create a vCPU instance for this thread */
    if (hv_vcpu_create(&vcpu, HV_VCPU_DEFAULT))
        abort();

    /*
     * From FreeBSD:
     *
     * It is safe to allow direct access to MSR_GSBASE and MSR_FSBASE.
     * The guest FSBASE and GSBASE are saved and restored during
     * vm-exit and vm-entry respectively. The host FSBASE and GSBASE are
     * always restored from the vmcs host state area on vm-exit.
     *
     * The SYSENTER_CS/ESP/EIP MSRs are identical to FS/GSBASE in
     * how they are saved/restored so can be directly accessed by the
     * guest.
     *
     * MSR_EFER is saved and restored in the guest VMCS area on a
     * VM exit and entry respectively. It is also restored from the
     * host VMCS area on a VM exit.
     */
    if (hv_vcpu_enable_native_msr(vcpu, MSR_GSBASE, 1) ||
        hv_vcpu_enable_native_msr(vcpu, MSR_FSBASE, 1) ||
        hv_vcpu_enable_native_msr(vcpu, MSR_SYSENTER_CS_MSR, 1) ||
        hv_vcpu_enable_native_msr(vcpu, MSR_SYSENTER_ESP_MSR, 1) ||
        hv_vcpu_enable_native_msr(vcpu, MSR_SYSENTER_EIP_MSR, 1) ||
        hv_vcpu_enable_native_msr(vcpu, MSR_LSTAR, 1) ||
        hv_vcpu_enable_native_msr(vcpu, MSR_CSTAR, 1) ||
        hv_vcpu_enable_native_msr(vcpu, MSR_STAR, 1) ||
        hv_vcpu_enable_native_msr(vcpu, MSR_SF_MASK, 1) ||
        hv_vcpu_enable_native_msr(vcpu, MSR_KGSBASE, 1)) {
        abort();
    }

    VMX_CTRLS(vcpu, HV_VMX_CAP_PINBASED, VMCS_CTRL_PIN_BASED, 0);

    /* It appears that bit 19 and 20 (CR8 load/store exiting) are
     * necessary for a bunch of things to work, including
     * CPU_BASED_HLT (bit 7) and MONITOR_TRAP_FLAG (bit 27)
     */
    if (0) {
    VMX_CTRLS(vcpu, HV_VMX_CAP_PROCBASED, VMCS_CTRL_CPU_BASED, 0
              | CPU_BASED_HLT | CPU_BASED_INVLPG
              | CPU_BASED_MWAIT | CPU_BASED_RDPMC
              | CPU_BASED_UNCOND_IO
              | CPU_BASED_CR8_LOAD | CPU_BASED_CR8_STORE
              | CPU_BASED_CR3_LOAD | CPU_BASED_CR3_STORE);
    } else {
    VMX_CTRLS(vcpu, HV_VMX_CAP_PROCBASED, VMCS_CTRL_CPU_BASED, 0
              | CPU_BASED_HLT | CPU_BASED_INVLPG
              | CPU_BASED_MWAIT | CPU_BASED_RDPMC
              | CPU_BASED_RDTSC | CPU_BASED_UNCOND_IO
              | CPU_BASED_CR8_LOAD | CPU_BASED_CR8_STORE
              | CPU_BASED_CR3_LOAD | CPU_BASED_CR3_STORE);
    }
    VMX_CTRLS(vcpu, HV_VMX_CAP_PROCBASED2, VMCS_CTRL_CPU_BASED2, 0
              | CPU_BASED2_DESC_TABLE | CPU_BASED2_RDRAND);
    VMX_CTRLS(vcpu, HV_VMX_CAP_ENTRY, VMCS_CTRL_VMENTRY_CONTROLS, 0
              | VMENTRY_GUEST_IA32E | VMENTRY_LOAD_EFER);
    VMX_CTRLS(vcpu, HV_VMX_CAP_EXIT, VMCS_CTRL_VMEXIT_CONTROLS, 0);

    wvmcs(vcpu, VMCS_CTRL_EXC_BITMAP, 0xffffffff);

    platform.mem = mem;
    platform.vcpu = vcpu;
    platform.priv = NULL;

    *pdata_p = &platform;

    return 0;
}

void platform_cleanup(struct platform *p)
{
    mach_port_deallocate(mach_task_self(), cclock);

    /* destroy vCPU */
    if (hv_vcpu_destroy(p->vcpu))
        abort();

    /* unmap memory segment at address 0 */
    if (hv_vm_unmap(0, GUEST_SIZE))
        abort();

    /* destroy VM instance of this task */
    if (hv_vm_destroy())
        abort();

    free(p->mem);
}

int platform_run(struct platform *p)
{
    return !!hv_vcpu_run(p->vcpu);
}

int platform_get_exit_reason(struct platform *p)
{
    uint64_t exit_reason = rvmcs(p->vcpu, VMCS_RO_EXIT_REASON);

    switch ((int)exit_reason) {
    case VMX_REASON_HLT:
        return EXIT_HLT;
    case VMX_REASON_RDTSC:
        return EXIT_RDTSC;
    case VMX_REASON_IO:
        return EXIT_IO;

    case VMX_REASON_IRQ:           /* host interrupt */
    case VMX_REASON_EPT_VIOLATION: /* cold misses */
        return EXIT_IGNORE;

    case VMX_REASON_EXC_NMI: {
        uint32_t idt_vector_info = rvmcs(p->vcpu, VMCS_RO_IDT_VECTOR_INFO);
        uint32_t idt_vector_error = rvmcs(p->vcpu, VMCS_RO_IDT_VECTOR_ERROR);
        uint32_t irq_info = rvmcs(p->vcpu, VMCS_RO_VMEXIT_IRQ_INFO);
        uint32_t irq_error = rvmcs(p->vcpu, VMCS_RO_VMEXIT_IRQ_ERROR);

        /* irq && HW exception && #DB */
        if (irq_info
            && (((irq_info >> 8) & 0x3) == 3)
            && ((irq_info & 0xff) == 1))
            return EXIT_DEBUG;

        printf("EXIT_REASON_EXCEPTION\n");
        if (idt_vector_info) {
            printf("idt_vector_info = 0x%x\n", idt_vector_info);
            printf("idt_vector_error = 0x%x\n", idt_vector_error);
        }
        if (irq_info) {
            printf("irq_info = 0x%x\n", irq_info);
            printf("  vector = %d (0x%x)\n",
                   irq_info & 0xff,
                   irq_info & 0xff);
            switch ((irq_info >> 8) & 0x3) {
            case 0:
                printf("  type = external\n");
                break;
            case 2:
                printf("  type = NMI\n");
                break;
            case 3:
                printf("  type = HW exception\n");
                break;
            case 6:
                printf("  type = SW exception\n");
                break;
            default:
                printf("  type = BOGUS!!!\n");
            }
            if ((irq_info >> 11) & 0x1)
                printf("irq_error = 0x%x\n", irq_error);
        }

        printf("RIP was 0x%llx\n", rreg(p->vcpu, HV_X86_RIP));
        printf("RSP was 0x%llx\n", rreg(p->vcpu, HV_X86_RSP));
        return EXIT_FAIL;
    }
    case VMX_REASON_VMENTRY_GUEST:
        fprintf(stderr, "Invalid VMCS!");
        return EXIT_FAIL;
    default:
        fprintf(stderr, "unhandled VMEXIT %lld (0x%llx)\n",
                exit_reason, exit_reason);
        fprintf(stderr, "RIP was 0x%llx\n", rreg(p->vcpu, HV_X86_RIP));
        return EXIT_FAIL;
    }
}

int platform_get_io_port(struct platform *p)
{
    uint64_t exit_qualification = rvmcs(p->vcpu, VMCS_RO_EXIT_QUALIFIC);
    uint16_t port = (uint16_t)(exit_qualification >> 16);

    return port;
}

uint64_t platform_get_io_data(struct platform *p)
{
    uint64_t rax = rreg(p->vcpu, HV_X86_RAX);

    return GUEST_PIO32_TO_PADDR(&rax);
}

void platform_advance_rip(struct platform *p)
{
    uint64_t len = rvmcs(p->vcpu, VMCS_RO_VMEXIT_INSTR_LEN);

    wvmcs(p->vcpu, VMCS_GUEST_RIP, rreg(p->vcpu, HV_X86_RIP) + len);
}

void platform_init_time(uint64_t *freq)
{
    size_t len = sizeof(*freq);

    host_get_clock_service(mach_host_self(),
                           CALENDAR_CLOCK, &cclock);

    sysctlbyname("machdep.tsc.frequency", freq, &len, NULL, 0);
}

uint64_t platform_get_exec_time(struct platform *p)
{
    uint64_t exec_time;

    if (hv_vcpu_get_exec_time(p->vcpu, &exec_time))
        errx(1, "couldn't get exec time");

    return exec_time;
}

void platform_emul_rdtsc(struct platform *p, uint64_t new_tsc)
{
    wreg(p->vcpu, HV_X86_RAX, new_tsc & 0xffffffff);
    wreg(p->vcpu, HV_X86_RDX, (new_tsc >> 32) & 0xffffffff);
}

void platform_get_timestamp(uint64_t *s, uint64_t *ns)
{
    mach_timespec_t mts;

    clock_get_time(cclock, &mts);
    *s = mts.tv_sec;
    *ns = mts.tv_nsec;
}
