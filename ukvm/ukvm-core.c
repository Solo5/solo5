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
#include <elf.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>
#include <poll.h>

#include "ukvm-private.h"
#include "ukvm-modules.h"
#include "ukvm-cpu.h"
#include "ukvm.h"

struct ukvm_module *modules[] = {
#ifdef UKVM_MODULE_BLK
    &ukvm_blk,
#endif
#ifdef UKVM_MODULE_NET
    &ukvm_net,
#endif
#ifdef UKVM_MODULE_GDB
    &ukvm_gdb,
#endif
    NULL,
};
#define NUM_MODULES ((sizeof(modules) / sizeof(struct ukvm_module *)) - 1)

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

#define BOOT_GDT     0x1000
#define BOOT_INFO    0x2000
#define BOOT_PML4    0x10000
#define BOOT_PDPTE   0x11000
#define BOOT_PDE     0x12000

#define BOOT_GDT_NULL    0
#define BOOT_GDT_CODE    1
#define BOOT_GDT_DATA    2
#define BOOT_GDT_MAX     3

#define KVM_32BIT_MAX_MEM_SIZE  (1ULL << 32)
#define KVM_32BIT_GAP_SIZE    (768 << 20)
#define KVM_32BIT_GAP_START    (KVM_32BIT_MAX_MEM_SIZE - KVM_32BIT_GAP_SIZE)

void setup_boot_info(uint8_t *mem,
                    uint64_t size,
                    uint64_t kernel_end,
                    int argc, char **argv)
{
    struct ukvm_boot_info *bi = (struct ukvm_boot_info *)(mem + BOOT_INFO);
    uint64_t cmdline = BOOT_INFO + sizeof(struct ukvm_boot_info);
    size_t cmdline_free = BOOT_PML4 - cmdline - 1;
    char *cmdline_p = (char *)(mem + cmdline);

    bi->mem_size = size;
    bi->kernel_end = kernel_end;
    bi->cmdline = cmdline;
    cmdline_p[0] = 0;

    for (; *argv; argc--, argv++) {
        size_t alen = snprintf(cmdline_p, cmdline_free, "%s%s", *argv,
                (argc > 1) ? " " : "");
        if (alen >= cmdline_free) {
            warnx("command line too long, truncated");
            break;
        }
        cmdline_free -= alen;
        cmdline_p += alen;
    }

}

ssize_t pread_in_full(int fd, void *buf, size_t count, off_t offset)
{
    ssize_t total = 0;
    char *p = buf;

    while (count > 0) {
        ssize_t nr;

        nr = pread(fd, p, count, offset);
        if (nr <= 0) {
            if (total > 0)
                return total;

            return -1;
        }

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
        err(1, "%s: Could not open file", file);

    numb = pread_in_full(fd_kernel, &hdr, sizeof(Elf64_Ehdr), 0);
    if (numb < 0)
        err(1, "%s: Could not read ELF header", file);
    if (numb != sizeof(Elf64_Ehdr))
        errx(1, "%s: Not a valid ELF executable", file);

    /*
     * Validate program is in ELF64 format:
     * 1. EI_MAG fields 0, 1, 2, 3 spell ELFMAG('0x7f', 'E', 'L', 'F'),
     * 2. File contains 64-bit objects,
     * 3. Objects are Executable,
     * 4. Target instruction set architecture is set to x86_64.
     */
    if (hdr.e_ident[EI_MAG0] != ELFMAG0 || hdr.e_ident[EI_MAG1] != ELFMAG1 || \
        hdr.e_ident[EI_MAG2] != ELFMAG2 || hdr.e_ident[EI_MAG3] != ELFMAG3 || \
        hdr.e_ident[EI_CLASS] != ELFCLASS64 || hdr.e_type != ET_EXEC || \
        hdr.e_machine != EM_X86_64)
        errx(1, "%s: Not a valid x86_64 ELF executable", file);

    ph_off = hdr.e_phoff;
    ph_entsz = hdr.e_phentsize;
    ph_cnt = hdr.e_phnum;
    buflen = ph_entsz * ph_cnt;

    phdr = malloc(buflen);
    if (!phdr)
        err(1, "Unable to allocate program header");

    numb = pread_in_full(fd_kernel, phdr, buflen, ph_off);
    if (numb < 0)
        err(1, "%s: Read error", file);
    if (numb != buflen)
        err(1, "%s: Invalid program header", file);

    /*
     * Load all segments with the LOAD directive from the elf file at offset
     * p_offset, and copy that into p_addr in memory. The amount of bytes
     * copied is p_filesz.  However, each segment should be given
     * ALIGN_UP(p_memsz, p_align) bytes on memory.
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

        /* XXX this won't work after having the two memory slots */
        assert(GUEST_SIZE < KVM_32BIT_GAP_SIZE);

        if ((paddr >= GUEST_SIZE) || add_overflow(paddr, filesz, result) ||
            (result >= GUEST_SIZE)) {
            errx(1, "%s: Invalid segment: paddr=0x%" PRIx64 ", filesz=%zu",
                    file, paddr, filesz);
        }
        if (add_overflow(paddr, memsz, result) || (result >= GUEST_SIZE)) {
            errx(1, "%s: Invalid segment: paddr=0x%" PRIx64 ", memsz=%zu",
                    file, paddr, memsz);
        }
        _end = ALIGN_UP(result, align);
        if (_end >= GUEST_SIZE) {
            errx(1, "%s: Invalid segment: paddr=0x%" PRIx64 \
                    ", _end=0x%" PRIx64, file, paddr, _end);
        }
        if (_end > *p_end)
            *p_end = _end;

        daddr = mem + paddr;
        numb = pread_in_full(fd_kernel, daddr, filesz, offset);
        if (numb < 0)
            err(1, "%s: Read error", file);
        if (numb != filesz)
            errx(1, "%s: Short read", file);
        memset(daddr + filesz, 0, memsz - filesz);

        /* Write-protect the executable segment */
        if (phdr[ph_i].p_flags & PF_X) {
            if (mprotect(daddr, _end - paddr, PROT_EXEC | PROT_READ) == -1)
                errx(1, "%s: mprotect() failed, daddr=%p, paddr=0x%lx",
                        file, daddr, paddr);
        }
    }

    close (fd_kernel);
    *p_entry = hdr.e_entry;
}


static void setup_system_64bit(struct kvm_sregs *sregs)
{
    sregs->cr0 |= X86_CR0_PE;
    sregs->efer |= EFER_LME;
}


static void setup_system_page_tables(struct kvm_sregs *sregs, uint8_t *mem)
{
    uint64_t *pml4 = (uint64_t *) (mem + BOOT_PML4);
    uint64_t *pdpte = (uint64_t *) (mem + BOOT_PDPTE);
    uint64_t *pde = (uint64_t *) (mem + BOOT_PDE);
    uint64_t paddr;

    /*
     * For simplicity we currently use 2MB pages and only a single
     * PML4/PDPTE/PDE.  Sanity check that the guest size is a multiple of the
     * page size and will fit in a single PDE (512 entries).
     */
    assert((GUEST_SIZE & (GUEST_PAGE_SIZE - 1)) == 0);
    assert(GUEST_SIZE <= (GUEST_PAGE_SIZE * 512));

    memset(pml4, 0, 4096);
    memset(pdpte, 0, 4096);
    memset(pde, 0, 4096);

    *pml4 = BOOT_PDPTE | (X86_PDPT_P | X86_PDPT_RW);
    *pdpte = BOOT_PDE | (X86_PDPT_P | X86_PDPT_RW);
    for (paddr = 0; paddr < GUEST_SIZE; paddr += GUEST_PAGE_SIZE, pde++)
        *pde = paddr | (X86_PDPT_P | X86_PDPT_RW | X86_PDPT_PS);

    sregs->cr3 = BOOT_PML4;
    sregs->cr4 |= X86_CR4_PAE;
    sregs->cr0 |= X86_CR0_PG;
}

static void setup_system_gdt(struct kvm_sregs *sregs,
                             uint8_t *mem,
                             uint64_t off)
{
    uint64_t *gdt = (uint64_t *) (mem + off);
    struct kvm_segment data_seg, code_seg;

    /* flags, base, limit */
    gdt[BOOT_GDT_NULL] = GDT_ENTRY(0, 0, 0);
    gdt[BOOT_GDT_CODE] = GDT_ENTRY(0xA09B, 0, 0xFFFFF);
    gdt[BOOT_GDT_DATA] = GDT_ENTRY(0xC093, 0, 0xFFFFF);

    sregs->gdt.base = off;
    sregs->gdt.limit = (sizeof(uint64_t) * BOOT_GDT_MAX) - 1;

    GDT_TO_KVM_SEGMENT(code_seg, gdt, BOOT_GDT_CODE);
    GDT_TO_KVM_SEGMENT(data_seg, gdt, BOOT_GDT_DATA);

    sregs->cs = code_seg;
    sregs->ds = data_seg;
    sregs->es = data_seg;
    sregs->fs = data_seg;
    sregs->gs = data_seg;
    sregs->ss = data_seg;
}

static void setup_system(int vcpufd, uint8_t *mem)
{
    struct kvm_sregs sregs;
    int ret;

    /* Set all cpu/mem system structures */
    ret = ioctl(vcpufd, KVM_GET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM: ioctl (GET_SREGS) failed");

    setup_system_gdt(&sregs, mem, BOOT_GDT);
    setup_system_page_tables(&sregs, mem);
    setup_system_64bit(&sregs);

    ret = ioctl(vcpufd, KVM_SET_SREGS, &sregs);
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

#if 0
static void inject_interrupt(int vcpufd, uint32_t intr)
{
    struct kvm_interrupt irq = { intr };

    int ret = ioctl(vcpufd, KVM_INTERRUPT, &irq);

    if (ret) {
        printf("ret = %d\n", ret);
        printf("errno = %d\n", errno);
        err(1, "KVM_INTERRUPT");
    }
}
#endif

static pthread_cond_t sleep_cv = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t interrupt_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *event_loop(void *arg)
{
    struct kvm_run *run = (struct kvm_run *) arg;

    /*
     * We are using a TSC-based clock, but this is an example for
     * delivering a timer interrupt once a second.
     */
    if (0) {
        for (;;) {
            sleep(1);		/* every 1 s */

            pthread_mutex_lock(&interrupt_mutex);

            run->request_interrupt_window = 1;
            pthread_cond_signal(&sleep_cv);

            pthread_mutex_unlock(&interrupt_mutex);
        }
    }

    return NULL;
}

void ukvm_port_puts(uint8_t *mem, uint64_t paddr)
{
    GUEST_CHECK_PADDR(paddr, GUEST_SIZE, sizeof (struct ukvm_puts));
    struct ukvm_puts *p = (struct ukvm_puts *)(mem + paddr);

    GUEST_CHECK_PADDR(p->data, GUEST_SIZE, p->len);
    assert(write(1, mem + p->data, p->len) != -1);
}

void ukvm_port_poll(uint8_t *mem, uint64_t paddr)
{
    GUEST_CHECK_PADDR(paddr, GUEST_SIZE, sizeof (struct ukvm_poll));
    struct ukvm_poll *t = (struct ukvm_poll *)(mem + paddr);
    struct timespec ts;
    int rc, i, num_fds = 0;
    struct pollfd fds[NUM_MODULES];  /* we only support at most one
                                      * instance per module for now
                                      */

    for (i = 0; i < NUM_MODULES; i++) {
        int fd = modules[i]->get_fd();

        if (fd) {
            fds[num_fds].fd = fd;
            fds[num_fds].events = POLLIN;
            num_fds += 1;
        }
    }

    ts.tv_sec = t->timeout_nsecs / 1000000000ULL;
    ts.tv_nsec = t->timeout_nsecs % 1000000000ULL;

    /*
     * Guest execution is blocked during the ppoll() call, note that
     * interrupts will not be injected.
     */
    rc = ppoll(fds, num_fds, &ts, NULL);
    assert(rc >= 0);
    t->ret = rc;
}

static int vcpu_loop(struct kvm_run *run, int vcpufd, uint8_t *mem)
{
    int ret;

    /* Repeatedly run code and handle VM exits. */
    while (1) {
        int i, handled = 0;

        ret = ioctl(vcpufd, KVM_RUN, NULL);
        if (ret == -1 && errno == EINTR)
            continue;
        if (ret == -1) {
            if (errno == EFAULT) {
                struct kvm_regs regs;
                ret = ioctl(vcpufd, KVM_GET_REGS, &regs);
                if (ret == -1)
                    err(1, "KVM: ioctl (GET_REGS) failed after guest fault");
                errx(1, "KVM: host/guest translation fault: rip=0x%llx",
                        regs.rip);
            }
            else
                err(1, "KVM: ioctl (RUN) failed");
        }

        for (i = 0; i < NUM_MODULES; i++) {
            if (!modules[i]->handle_exit(run, vcpufd, mem)) {
                handled = 1;
                break;
            }
        }

        if (handled)
            continue;

        switch (run->exit_reason) {
        case KVM_EXIT_HLT: {
            /* Guest has halted the CPU, this is considered as a normal exit. */
            return 0;
        }
        case KVM_EXIT_IO: {
            assert(run->io.direction == KVM_EXIT_IO_OUT);
            assert(run->io.size == 4);
            uint64_t paddr =
                GUEST_PIO32_TO_PADDR((uint8_t *)run + run->io.data_offset);

            switch (run->io.port) {
            case UKVM_PORT_PUTS:
                ukvm_port_puts(mem, paddr);
                break;
            case UKVM_PORT_POLL:
                ukvm_port_poll(mem, paddr);
                break;
            default:
                errx(1, "Invalid guest port access: port=0x%x", run->io.port);
            };
            break;
        }
        case KVM_EXIT_IRQ_WINDOW_OPEN: {
            run->request_interrupt_window = 0;
            /* inject_interrupt(vcpufd, INTR_USER_TIMER); */
            /* inject_interrupt(vcpufd, 0x31); */
            break;
        }
        case KVM_EXIT_INTR: {
            /* RUN was interrupted, so we just resume */
            /* note, this was probably because we are going to put an
             * interrupt in, so there might be some efficiency to get
             * there
             */
            break;
        }
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

    return 0; /* XXX Refactor return code paths in the above code */
}

int setup_modules(int vcpufd, uint8_t *mem)
{
    int i;

    for (i = 0; i < NUM_MODULES; i++) {
        if (modules[i]->setup(vcpufd, mem)) {
            warnx("Module `%s' setup failed", modules[i]->name);
            warnx("Please check you have correctly specified:\n    %s",
                   modules[i]->usage());
            return -1;
        }
    }
    return 0;
}

void sig_handler(int signo)
{
    warnx("Exiting on signal %d", signo);
    exit(0);
}

static void usage(const char *prog)
{
    int m;

    fprintf(stderr, "usage: %s [ CORE OPTIONS ] [ MODULE OPTIONS ] KERNEL", prog);
    fprintf(stderr, " [ -- ] [ ARGS ]\n");
    fprintf(stderr, "Core options:\n");
    fprintf(stderr, "    --help (display this help)\n");
    fprintf(stderr, "Compiled-in module options:\n");
    for (m = 0; m < NUM_MODULES; m++)
        fprintf(stderr, "    %s\n", modules[m]->usage());
    if (!m)
        fprintf(stderr, "    (none)\n");
    exit(1);
}

int main(int argc, char **argv)
{
    int kvm, vmfd, vcpufd, ret;
    uint8_t *mem;
    struct kvm_run *run;
    size_t mmap_size;
    uint64_t elf_entry;
    uint64_t kernel_end;
    const char *prog;
    const char *elffile;
    int matched;

    prog = basename(*argv);
    argc--;
    argv++;

    if (argc < 1) {
        warnx("Missing KERNEL operand");
        usage(prog);
    }

    do {
        int j;

        if (!strcmp("--help", *argv))
            usage(prog);

        matched = 0;
        for (j = 0; j < NUM_MODULES; j++) {
            if (!modules[j]->handle_cmdarg(*argv)) {
                matched = 1;
                argc--;
                argv++;
                break;
            }
        }
    } while (matched && *argv);

    if (!*argv)
        usage(prog);

    if (*argv[0] == '-') {
        warnx("Invalid option: `%s'", *argv);
        usage(prog);
    }

    elffile = *argv;
    argc--;
    argv++;

    if (argc) {
        if (strcmp("--", *argv))
            usage(prog);
        argc--;
        argv++;
    }

    if (signal(SIGINT, sig_handler) == SIG_ERR)
        err(1, "Could not install signal handler");

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

    load_code(elffile, mem, &elf_entry, &kernel_end);

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

    /* Setup x86 system registers and memory. */
    setup_system(vcpufd, mem);

    /* Setup ukvm_boot_info and command line */
    setup_boot_info(mem, GUEST_SIZE, kernel_end, argc, argv);

    /*
     * Initialize registers: instruction pointer for our code, addends,
     * and initial flags required by x86 architecture.
     * Arguments to the kernel main are passed using the x86_64 calling
     * convention: RDI, RSI, RDX, RCX, R8, and R9
     */
    struct kvm_regs regs = {
        .rip = elf_entry,
        .rax = 2,
        .rbx = 2,
        .rflags = 0x2,
        .rsp = GUEST_SIZE - 8,  /* x86_64 ABI requires ((rsp + 8) % 16) == 0 */
        .rdi = BOOT_INFO,       /* size arg in kernel main */
    };
    ret = ioctl(vcpufd, KVM_SET_REGS, &regs);
    if (ret == -1)
        err(1, "KVM: ioctl (SET_REGS) failed");


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

    /* start event thread */
    pthread_t event_thread;

    ret = pthread_create(&event_thread, NULL, event_loop, (void *) run);
    if (ret)
        err(1, "pthread_create(event_thread) failed");

    if (setup_modules(vcpufd, mem))
        exit(1);

    return vcpu_loop(run, vcpufd, mem);
}
