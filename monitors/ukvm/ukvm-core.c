/* Copyright (c) 2015, IBM
 * Author(s): Dan Williams <djwillia@us.ibm.com>
 *            Ricardo Koller <kollerr@us.ibm.com>
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
 *   kvmtest.c: http://lwn.net/Articles/658512/
 *   lkvm: http://github.com/clearlinux/kvmtool
 */
#define _GNU_SOURCE
#include <err.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/const.h>
#include <asm/msr-index.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <poll.h>
#include <limits.h>
#include <time.h>
#include <elf.h>

#include "../unikernel-monitor.h"
#include "../ukvm-private.h"
#include "../ukvm-cpu.h"

#define KVM_32BIT_MAX_MEM_SIZE  (1ULL << 32)
#define KVM_32BIT_GAP_SIZE    (768 << 20)
#define KVM_32BIT_GAP_START    (KVM_32BIT_MAX_MEM_SIZE - KVM_32BIT_GAP_SIZE)

static struct platform platform;

static ssize_t pread_in_full(int fd, void *buf, size_t count, off_t offset)
{
    ssize_t total = 0;
    char *p = buf;

    if (count > SSIZE_MAX) {
        errno = E2BIG;
        return -1;
    }

    lseek(fd, 0, SEEK_SET);
    while (count > 0) {
        ssize_t nr;

        lseek(fd, offset, SEEK_SET);
        nr = read(fd, p, count);
        if (nr == 0)
            return total;
        else if (nr == -1 && errno == EINTR)
            continue;
        else if (nr == -1)
            return -1;

        count -= nr;
        total += nr;
        p += nr;
        offset += nr;
    }

    return total;
}

/*
 * Load code from elf file into *mem and return the elf entry point
 * and the last byte of the program when loaded into memory. This
 * accounts not only for the last loaded piece of code from the elf,
 * but also for the zeroed out pieces that are not loaded and sould be
 * reserved.
 *
 * Memory will look like this after the elf is loaded:
 *
 * *mem                    *p_entry                   *p_end
 *   |             |                    |                |
 *   |    ...      | .text .rodata      |   .data .bss   |
 *   |             |        code        |   00000000000  |
 *   |             |  [PROT_EXEC|READ]  |                |
 *
 */
static void load_code(const char *file, uint8_t *mem,     /* IN */
                      uint64_t *p_entry, uint64_t *p_end) /* OUT */
{
    int fd_kernel;
    ssize_t numb;
    size_t buflen;
    Elf64_Off ph_off;
    Elf64_Half ph_entsz;
    Elf64_Half ph_cnt;
    Elf64_Half ph_i;
    Elf64_Phdr *phdr = NULL;
    Elf64_Ehdr hdr;

    /* elf entry point (on physical memory) */
    *p_entry = 0;
    /* highest byte of the program (on physical memory) */
    *p_end = 0;

    fd_kernel = open(file, O_RDONLY);
    if (fd_kernel == -1)
        goto out_error;

    numb = pread_in_full(fd_kernel, &hdr, sizeof(Elf64_Ehdr), 0);
    if (numb < 0)
        goto out_error;
    if (numb != sizeof(Elf64_Ehdr))
        goto out_invalid;

    /*
     * Validate program is in ELF64 format:
     * 1. EI_MAG fields 0, 1, 2, 3 spell ELFMAG('0x7f', 'E', 'L', 'F'),
     * 2. File contains 64-bit objects,
     * 3. Objects are Executable,
     * 4. Target instruction set architecture is set to x86_64.
     */
    if (hdr.e_ident[EI_MAG0] != ELFMAG0
            || hdr.e_ident[EI_MAG1] != ELFMAG1
            || hdr.e_ident[EI_MAG2] != ELFMAG2
            || hdr.e_ident[EI_MAG3] != ELFMAG3
            || hdr.e_ident[EI_CLASS] != ELFCLASS64
            || hdr.e_type != ET_EXEC
            || hdr.e_machine != EM_X86_64)
        goto out_invalid;

    ph_off = hdr.e_phoff;
    ph_entsz = hdr.e_phentsize;
    ph_cnt = hdr.e_phnum;
    buflen = ph_entsz * ph_cnt;

    phdr = (Elf64_Phdr *)malloc(buflen);
    if (!phdr)
        goto out_error;
    numb = pread_in_full(fd_kernel, phdr, buflen, ph_off);
    if (numb < 0)
        goto out_error;
    if (numb != buflen)
        goto out_invalid;

    /*
     * Load all segments with the LOAD directive from the elf file at offset
     * p_offset, and copy that into p_addr in memory. The amount of bytes
     * copied is p_filesz.  However, each segment should be given
     * p_memsz aligned up to p_align bytes on memory.
     */
    for (ph_i = 0; ph_i < ph_cnt; ph_i++) {
        uint8_t *daddr;
        uint64_t _end;
        size_t offset = phdr[ph_i].p_offset;
        size_t filesz = phdr[ph_i].p_filesz;
        size_t memsz = phdr[ph_i].p_memsz;
        uint64_t paddr = phdr[ph_i].p_paddr;
        uint64_t align = phdr[ph_i].p_align;
        uint64_t result;

        if (phdr[ph_i].p_type != PT_LOAD)
            continue;

        if ((paddr >= GUEST_SIZE) || add_overflow(paddr, filesz, result)
                || (result >= GUEST_SIZE))
            goto out_invalid;
        if (add_overflow(paddr, memsz, result) || (result >= GUEST_SIZE))
            goto out_invalid;
        /*
         * Verify that align is a non-zero power of 2 and safely compute
         * ((_end + (align - 1)) & -align).
         */
        if (align > 0 && (align & (align - 1)) == 0) {
            if (add_overflow(result, (align - 1), _end))
                goto out_invalid;
            _end = _end & -align;
        } else {
            _end = result;
        }
        if (_end > *p_end)
            *p_end = _end;

        daddr = p->mem + paddr;
        numb = pread_in_full(fd_kernel, daddr, filesz, offset);
        if (numb < 0)
            goto out_error;
        if (numb != filesz)
            goto out_invalid;
        memset(daddr + filesz, 0, memsz - filesz);

        /* Write-protect the executable segment */
        if (phdr[ph_i].p_flags & PF_X) {
            if (mprotect(daddr, _end - paddr, PROT_EXEC | PROT_READ) == -1)
                goto out_error;
        }
    }

    free(phdr);
    close(fd_kernel);
    *p_entry = hdr.e_entry;
    return;

out_error:
    err(1, "%s", file);

out_invalid:
    errx(1, "%s: Exec format error", file);
}

void platform_setup_system_64bit(struct platform *p, uint64_t cr0,
                                 uint64_t cr4, uint64_t efer)
{
    struct kvm_sregs sregs;
    int ret;

    ret = ioctl(p->vcpu, KVM_GET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM: ioctl (GET_SREGS) failed");

    sregs.cr0 = cr0;
    sregs.cr4 = cr4;
    sregs.efer = efer;

    ret = ioctl(p->vcpu, KVM_SET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM: ioctl (SET_SREGS) failed");
}

void platform_setup_system_page_tables(struct platform *p,
                                       uint64_t pml4)
{
    struct kvm_sregs sregs;
    int ret;

    ret = ioctl(p->vcpu, KVM_GET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM: ioctl (GET_SREGS) failed");

    sregs.cr3 = pml4;

    ret = ioctl(p->vcpu, KVM_SET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM: ioctl (SET_SREGS) failed");
}

void platform_setup_system_gdt(struct platform *p,
                               uint64_t cs_idx,
                               uint64_t ds_idx,
                               uint64_t off,
                               uint64_t limit)
{
    struct kvm_sregs sregs;
    struct kvm_segment data_seg, code_seg;
    int ret;
    uint64_t *gdt = (uint64_t *) (p->mem + off);

    /* Set all cpu/mem system structures */
    ret = ioctl(p->vcpu, KVM_GET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM: ioctl (GET_SREGS) failed");

    sregs.gdt.base = off;
    sregs.gdt.limit = limit;

    GDT_TO_KVM_SEGMENT(code_seg, gdt, cs_idx);
    GDT_TO_KVM_SEGMENT(data_seg, gdt, ds_idx);

    sregs.cs = code_seg;
    sregs.ds = data_seg;
    sregs.es = data_seg;
    sregs.fs = data_seg;
    sregs.gs = data_seg;
    sregs.ss = data_seg;

    ret = ioctl(p->vcpu, KVM_SET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM: ioctl (SET_SREGS) failed");
}

static void setup_cpuid(int kvm, int vcpufd)
{
    struct kvm_cpuid2 *kvm_cpuid;
    int max_entries = 100;

    kvm_cpuid = calloc(1, sizeof(*kvm_cpuid) +
                          max_entries * sizeof(*kvm_cpuid->entries));
    kvm_cpuid->nent = max_entries;

    if (ioctl(kvm, KVM_GET_SUPPORTED_CPUID, kvm_cpuid) < 0)
        err(1, "KVM: ioctl (GET_SUPPORTED_CPUID) failed");

    if (ioctl(vcpufd, KVM_SET_CPUID2, kvm_cpuid) < 0)
        err(1, "KVM: ioctl (SET_CPUID2) failed");
}

int platform_run(struct platform *p)
{
    while (1) {
        int ret;

        ret = ioctl(p->vcpu, KVM_RUN, NULL);
        if (ret == -1 && errno == EINTR)
            continue;
        if (ret == -1) {
            if (errno == EFAULT) {
                struct kvm_regs regs;
                ret = ioctl(p->vcpu, KVM_GET_REGS, &regs);
                if (ret == -1)
                    err(1, "KVM: ioctl (GET_REGS) failed after guest fault");
                errx(1, "KVM: host/guest translation fault: rip=0x%llx",
                         regs.rip);
            } else {
                err(1, "KVM: ioctl (RUN) failed");
            }
        }

        return 0;
    }
}

int platform_get_io_port(struct platform *p)
{
    struct kvm_run *run = (struct kvm_run *)p->priv;

    if (run->io.direction != KVM_EXIT_IO_OUT
        || run->io.size != 4)
        errx(1, "Invalid guest port access: port=0x%x", run->io.port);

    return run->io.port;
}

uint64_t platform_get_io_data(struct platform *p)
{
    struct kvm_run *run = (struct kvm_run *)p->priv;

    assert(run->io.direction == KVM_EXIT_IO_OUT);
    assert(run->io.size == 4);

    return GUEST_PIO32_TO_PADDR((uint8_t *)run + run->io.data_offset);
}


int platform_get_exit_reason(struct platform *p)
{
    struct kvm_run *run = (struct kvm_run *)p->priv;

    switch (run->exit_reason) {
    case KVM_EXIT_HLT:
        return EXIT_HLT;

    case KVM_EXIT_IO:
        return EXIT_IO;

    case KVM_EXIT_INTR:
        return EXIT_IGNORE;

    case KVM_EXIT_DEBUG:
        return EXIT_DEBUG;

    case KVM_EXIT_FAIL_ENTRY:
        errx(1, "KVM: entry failure: hw_entry_failure_reason=0x%llx",
             run->fail_entry.hardware_entry_failure_reason);

    case KVM_EXIT_INTERNAL_ERROR:
        errx(1, "KVM: internal error exit: suberror=0x%x",
             run->internal.suberror);

    default:
        errx(1, "KVM: unhandled exit: exit_reason=0x%x", run->exit_reason);
    }
}

int platform_init(struct platform **pdata_p)
{
    int kvm, ret, vmfd, vcpufd;
    uint8_t *mem;
    struct kvm_run *run;
    size_t mmap_size;

    kvm = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    if (kvm == -1)
        err(1, "Could not open: /dev/kvm");

    /* Make sure we have the stable version of the API */
    ret = ioctl(kvm, KVM_GET_API_VERSION, NULL);
    if (ret == -1)
        err(1, "KVM: ioctl (GET_API_VERSION) failed");
    if (ret != 12)
        errx(1, "KVM: API version is %d, ukvm requires version 12", ret);

    vmfd = ioctl(kvm, KVM_CREATE_VM, 0);
    if (vmfd == -1)
        err(1, "KVM: ioctl (CREATE_VM) failed");

    /*
     * TODO If the guest size is larger than ~4GB, we need two region
     * slots: one before the pci gap, and one after it.
     * Reference: kvmtool x86/kvm.c:kvm__init_ram()
     */
    assert(GUEST_SIZE < KVM_32BIT_GAP_SIZE);

    /* Allocate GUEST_SIZE page-aligned guest memory. */
    mem = mmap(NULL, GUEST_SIZE, PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED)
        err(1, "Error allocating guest memory");

    struct kvm_userspace_memory_region region = {
        .slot = 0,
        .guest_phys_addr = 0,
        .memory_size = GUEST_SIZE,
        .userspace_addr = (uint64_t) mem,
    };

    ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &region);
    if (ret == -1)
        err(1, "KVM: ioctl (SET_USER_MEMORY_REGION) failed");


    /* enabling this seems to mess up our receiving of hlt instructions */
    /* ret = ioctl(vmfd, KVM_CREATE_IRQCHIP); */
    /* if (ret == -1) */
    /*     err(1, "KVM_CREATE_IRQCHIP"); */

    vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, 0);
    if (vcpufd == -1)
        err(1, "KVM: ioctl (CREATE_VCPU) failed");

    /* Map the shared kvm_run structure and following data. */
    ret = ioctl(kvm, KVM_GET_VCPU_MMAP_SIZE, NULL);
    if (ret == -1)
        err(1, "KVM: ioctl (GET_VCPU_MMAP_SIZE) failed");
    mmap_size = ret;
    if (mmap_size < sizeof(*run))
        errx(1, "KVM: invalid VCPU_MMAP_SIZE: %zd", mmap_size);
    run =
        mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpufd,
             0);
    if (run == MAP_FAILED)
        err(1, "KVM: VCPU mmap failed");

    setup_cpuid(kvm, vcpufd);

    platform.mem = mem;
    platform.vcpu = vcpufd;
    platform.priv = run;

    *pdata_p = &platform;

    return 0;
}

void platform_setup_system(struct platform *p, uint64_t entry,
                           uint64_t boot_info)
{
    int ret;
    /*
     * Initialize registers: instruction pointer for our code, addends,
     * and initial flags required by x86 architecture.
     * Arguments to the kernel main are passed using the x86_64 calling
     * convention: RDI, RSI, RDX, RCX, R8, and R9
     */
    struct kvm_regs regs = {
        .rip = entry,
        .rflags = 0x2,
        .rsp = GUEST_SIZE - 8,  /* x86_64 ABI requires ((rsp + 8) % 16) == 0 */
        .rdi = boot_info,       /* size arg in kernel main */
    };
    ret = ioctl(p->vcpu, KVM_SET_REGS, &regs);
    if (ret == -1)
        err(1, "KVM: ioctl (SET_REGS) failed");
}

void platform_cleanup(struct platform *p)
{
    /* XXX */
}

void platform_advance_rip(struct platform *p)
{
    /* no-op: KVM automatically advances RIP after I/O */
}

/* XXX this is horrible */
static uint64_t get_tsc_const(void)
{
    FILE *f = fopen("/proc/cpuinfo", "r");
    uint64_t mhz = 0, dec = 0;
    int ret = 0;

    assert(f != NULL);

    while (ret == 0) {
        ret = fscanf(f, "cpu MHz\t: %lu.%lu\n", &mhz, &dec);
        if (ret == 0) {
            while (fgetc(f) != '\n')
                fgetc(f);
        }
    }
    return (mhz * 1000000) + (dec * 1000);
}

void platform_init_time(uint64_t *freq)
{
    *freq = get_tsc_const();
}

uint64_t platform_get_exec_time(struct platform *p)
{
    printf("unimplemented");
    assert(0);
    return 0;
}

void platform_emul_rdtsc(struct platform *p, uint64_t new_tsc)
{
    printf("unimplemented");
    assert(0);
}

void platform_get_timestamp(uint64_t *s, uint64_t *ns)
{
    struct timespec tp;

    clock_gettime(CLOCK_REALTIME, &tp);
    *s = tp.tv_sec;
    *ns = tp.tv_nsec;
}
