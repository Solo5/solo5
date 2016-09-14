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

#include "processor-flags.h"
//#include "../kernel/interrupts.h"
#include "ukvm.h"
#include "ukvm_modules.h"
#include "misc.h"

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

#define GUEST_PAGE_SIZE 0x200000   /* 2 MB pages in guest */

#define BOOT_GDT     0x1000
#define BOOT_INFO    0x2000
#define BOOT_PML4    0x10000
#define BOOT_PDPTE   0x11000
#define BOOT_PDE     0x12000

#define BOOT_GDT_NULL    0
#define BOOT_GDT_CODE    1
#define BOOT_GDT_DATA    2
#define BOOT_GDT_MAX     3
#define GDT_DESC_OFFSET(n) ((n) * 0x8)

#define GDT_GET_BASE(x) (                      \
    (((x) & 0xFF00000000000000) >> 32) |       \
    (((x) & 0x000000FF00000000) >> 16) |       \
    (((x) & 0x00000000FFFF0000) >> 16))

#define GDT_GET_LIMIT(x) (__u32)(                                      \
                                 (((x) & 0x000F000000000000) >> 32) |  \
                                 (((x) & 0x000000000000FFFF)))

/* Constructor for a conventional segment GDT (or LDT) entry */
/* This is a macro so it can be used in initializers */
#define GDT_ENTRY(flags, base, limit)               \
    ((((base)  & _AC(0xff000000, ULL)) << (56-24)) | \
     (((flags) & _AC(0x0000f0ff, ULL)) << 40) |      \
     (((limit) & _AC(0x000f0000, ULL)) << (48-16)) | \
     (((base)  & _AC(0x00ffffff, ULL)) << 16) |      \
     (((limit) & _AC(0x0000ffff, ULL))))

struct _kvm_segment {
    __u64 base;
    __u32 limit;
    __u16 selector;
    __u8 type;
    __u8 present, dpl, db, s, l, g, avl;
    __u8 unusable;
    __u8 padding;
};


#define GDT_GET_G(x)   (__u8)(((x) & 0x0080000000000000) >> 55)
#define GDT_GET_DB(x)  (__u8)(((x) & 0x0040000000000000) >> 54)
#define GDT_GET_L(x)   (__u8)(((x) & 0x0020000000000000) >> 53)
#define GDT_GET_AVL(x) (__u8)(((x) & 0x0010000000000000) >> 52)
#define GDT_GET_P(x)   (__u8)(((x) & 0x0000800000000000) >> 47)
#define GDT_GET_DPL(x) (__u8)(((x) & 0x0000600000000000) >> 45)
#define GDT_GET_S(x)   (__u8)(((x) & 0x0000100000000000) >> 44)
#define GDT_GET_TYPE(x)(__u8)(((x) & 0x00000F0000000000) >> 40)



#define GDT_TO_KVM_SEGMENT(seg, gdt_table, sel) \
    do {                                        \
        __u64 gdt_ent = gdt_table[sel];         \
        seg.base = GDT_GET_BASE(gdt_ent);       \
        seg.limit = GDT_GET_LIMIT(gdt_ent);     \
        seg.selector = sel * 8;                 \
        seg.type = GDT_GET_TYPE(gdt_ent);       \
        seg.present = GDT_GET_P(gdt_ent);       \
        seg.dpl = GDT_GET_DPL(gdt_ent);         \
        seg.db = GDT_GET_DB(gdt_ent);           \
        seg.s = GDT_GET_S(gdt_ent);             \
        seg.l = GDT_GET_L(gdt_ent);             \
        seg.g = GDT_GET_G(gdt_ent);             \
        seg.avl = GDT_GET_AVL(gdt_ent);         \
    } while (0)

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
 * *mem                    *p_entry                                 *p_end
 *   |             |                    |                |            |
 *   |    ...      | .text .rodata      |   .data .bss   |            |
 *   |             |        code        |   00000000000  | empty page |
 *   |             |  [PROT_EXEC|READ]  |                | PROT_NONE  |
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
        err(1, "couldn't open elf");

    numb = pread_in_full(fd_kernel, &hdr, sizeof(Elf64_Ehdr), 0);
    if (numb < 0 || (size_t) numb != sizeof(Elf64_Ehdr))
        err(1, "unable to read ELF64 hdr");

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
        errx(1, "%s is in invalid ELF64 format.", file);

    ph_off = hdr.e_phoff;
    ph_entsz = hdr.e_phentsize;
    ph_cnt = hdr.e_phnum;
    buflen = ph_entsz * ph_cnt;

    phdr = malloc(buflen);
    if (!phdr)
        err(1, "unable to allocate program header buffer\n");

    numb = pread_in_full(fd_kernel, phdr, buflen, ph_off);
    if (numb < 0 || (size_t) numb != buflen)
        err(1, "unable to read program header");

    /*
     * Load all segments with the LOAD directive from the elf file at offset
     * p_offset, and copy that into p_addr in memory. The amount of bytes
     * copied is p_filesz.  However, each segment should be given
     * ALIGN_UP(p_memsz, p_align) bytes on memory.
     */
    for (ph_i = 0; ph_i < ph_cnt; ph_i++) {
        uint8_t *dst;
        size_t _end;
        size_t offset = phdr[ph_i].p_offset;
        size_t filesz = phdr[ph_i].p_filesz;
        size_t memsz = phdr[ph_i].p_memsz;
        uint64_t paddr = phdr[ph_i].p_paddr;
        uint64_t align = phdr[ph_i].p_align;

        if ((phdr[ph_i].p_type & PT_LOAD) == 0)
            continue;

        /* XXX this won't work after having the two memory slots */
        assert(GUEST_SIZE < KVM_32BIT_GAP_SIZE);
        dst = mem + paddr;

        numb = pread_in_full(fd_kernel, dst, filesz, offset);
        if (numb < 0 || (size_t) numb != filesz)
            err(1, "unable to load segment");

        memset(mem + paddr + filesz, 0, memsz - filesz);

        /* Protect the executable code */
        if (phdr[ph_i].p_flags & ELF_SEGMENT_X)
            mprotect((void *) dst, memsz, PROT_EXEC | PROT_READ);

        _end = ALIGN_UP(paddr + memsz, align);
        if (_end > *p_end)
            *p_end = _end;
    }

    /*
     * Not needed, but let's give it an empty page at the end for "safety".
     * And, even protect it against any type of access.
     */
    mprotect((void *) ((uint64_t) mem + p_end), 0x1000, PROT_NONE);
    *p_end += 0x1000;

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

#if 0
static void print_dtable(const char *name, struct kvm_dtable *dtable)
{
    printf(" %s                 %016lx  %08hx\n",
           name, (uint64_t) dtable->base, (uint16_t) dtable->limit);
}

static void print_segment(const char *name, struct kvm_segment *seg)
{
    printf(" %s       %04hx      %016lx  %08x  %02hhx    ",
           name, (uint16_t) seg->selector, (uint64_t) seg->base,
           (uint32_t) seg->limit, (uint8_t) seg->type);
    printf("%x %x   %x  %x %x %x %x\n",
           seg->present, seg->dpl, seg->db, seg->s,
           seg->l, seg->g, seg->avl);
}

static void dump_sregs(struct kvm_sregs *sregs)
{
    uint64_t cr0, cr2, cr3;
    uint64_t cr4, cr8;

    cr0 = sregs->cr0;
    cr2 = sregs->cr2;
    cr3 = sregs->cr3;
    cr4 = sregs->cr4;
    cr8 = sregs->cr8;

    printf(" cr0: %016lx   cr2: %016lx   cr3: %016lx\n", cr0, cr2, cr3);
    printf(" cr4: %016lx   cr8: %016lx\n", cr4, cr8);
    printf("\n Segment registers:\n");
    printf(" ------------------\n");
    printf(" register  selector  base             ");
    printf(" limit     type  p dpl db s l g avl\n");

    print_segment("cs ", &sregs->cs);
    print_segment("ss ", &sregs->ss);
    print_segment("ds ", &sregs->ds);
    print_segment("es ", &sregs->es);
    print_segment("fs ", &sregs->fs);
    print_segment("gs ", &sregs->gs);
    print_segment("tr ", &sregs->tr);
    print_segment("ldt", &sregs->ldt);
    print_dtable("gdt", &sregs->gdt);
}

static void get_and_dump_sregs(int vcpufd)
{
    struct kvm_sregs sregs;
    int ret;

    ret = ioctl(vcpufd, KVM_GET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM_GET_SREGS");

    dump_sregs(&sregs);
}
#endif

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

    if (0) {
        size_t i;

        printf("gdt @ 0x%x\n", BOOT_GDT);
        for (i = 0; i < 40; i++) {
            printf("%02x", *((uint8_t *) gdt + i));
            if (i % 8 == 7)
                printf("\n");
        }
    }

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
        err(1, "KVM_GET_SREGS");

    setup_system_gdt(&sregs, mem, BOOT_GDT);
    setup_system_page_tables(&sregs, mem);
    setup_system_64bit(&sregs);

    /* dump_sregs(&sregs); */

    ret = ioctl(vcpufd, KVM_SET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM_SET_SREGS");
}


static void setup_cpuid(int kvm, int vcpufd)
{
    struct kvm_cpuid2 *kvm_cpuid;
    int max_entries = 100;

    kvm_cpuid = calloc(1, sizeof(*kvm_cpuid) +
                          max_entries * sizeof(*kvm_cpuid->entries));
    kvm_cpuid->nent = max_entries;

    if (ioctl(kvm, KVM_GET_SUPPORTED_CPUID, kvm_cpuid) < 0)
        err(1, "KVM_GET_SUPPORTED_CPUID failed");

    printf("Setting the CPUID.\n");
    if (ioctl(vcpufd, KVM_SET_CPUID2, kvm_cpuid) < 0)
        err(1, "KVM_SET_CPUID2 failed");
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


void ukvm_port_puts(uint8_t *mem, void *data)
{
    uint32_t mem_off = *(uint32_t *) data;
    struct ukvm_puts *p = (struct ukvm_puts *) (mem + mem_off);

    printf("%.*s", p->len, (char *) (mem + (uint64_t) p->data));
    /* putchar(*(char *)data); */
}


void ukvm_port_nanosleep(uint8_t *mem, void *data, struct kvm_run *run)
{
    uint32_t arg_addr = *(uint32_t *) data;
    struct ukvm_nanosleep *t = (struct ukvm_nanosleep *) (mem + arg_addr);
    struct timespec now, ts;

    clock_gettime(CLOCK_REALTIME, &now);

    ts.tv_sec = now.tv_sec + t->sec_in;
    ts.tv_nsec = now.tv_nsec + t->nsec_in;
    if (ts.tv_nsec > 1000000000) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000;
    }

    pthread_mutex_lock(&interrupt_mutex);

    /* check if an interrupt is waiting */
    if (!run->request_interrupt_window)
        pthread_cond_timedwait(&sleep_cv, &interrupt_mutex, &ts);

    pthread_mutex_unlock(&interrupt_mutex);

    clock_gettime(CLOCK_REALTIME, &now);

    /* return the remaining time we should have slept */
    if (now.tv_sec < ts.tv_sec) {
        if (ts.tv_nsec < now.tv_nsec) {
            ts.tv_sec -= 1;
            ts.tv_nsec += 1000000000;
        }
        t->sec_out = ts.tv_sec - now.tv_sec;
        t->nsec_out = ts.tv_nsec - now.tv_nsec;
        t->ret = -1;
    } else if ((now.tv_sec == ts.tv_sec) && (now.tv_nsec < ts.tv_nsec)) {
        t->sec_out = 0;
        t->nsec_out = ts.tv_nsec - now.tv_nsec;
        t->ret = -1;
    } else {
        t->sec_out = 0;
        t->nsec_out = 0;
        t->ret = 0;
    }
}

void ukvm_port_dbg_stack(uint8_t *mem, int vcpufd)
{
    struct kvm_regs regs;
    int ret;
    uint64_t i;
    int cnt = 0;

    ret = ioctl(vcpufd, KVM_GET_REGS, &regs);
    if (ret == -1)
        err(1, "KVM_GET_REGS");

    /* crazy stack trace hints... */
    i = regs.rbp + 1024;
#define DBG_DEPTH 20
    while ((i < GUEST_SIZE) && (cnt++ < DBG_DEPTH)) {
        if (*(uint64_t *)(mem + i) != 0)
            printf("0x%lx: 0x%lx\n", i, *(uint64_t *)(mem + i));
        i -= 8;
    }
}

void ukvm_port_poll(uint8_t *mem, void *data)
{
    uint32_t arg_addr = *(uint32_t *) data;
    struct ukvm_poll *t = (struct ukvm_poll *) (mem + arg_addr);
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
        if (ret < 0 &&
                (errno != EINTR && errno != EAGAIN)) {
            err(1, "KVM_RUN failed");
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
            puts("KVM_EXIT_HLT");
            /* get_and_dump_sregs(vcpufd); */
            return 0;
        }
        case KVM_EXIT_IO: {
            uint8_t *data = (uint8_t *)run + run->io.data_offset;

            assert(run->io.direction == KVM_EXIT_IO_OUT);

            switch (run->io.port) {
            case UKVM_PORT_PUTS:
                ukvm_port_puts(mem, data);
                break;
            case UKVM_PORT_NANOSLEEP:
                ukvm_port_nanosleep(mem, data, run);
                break;
            case UKVM_PORT_DBG_STACK:
                ukvm_port_dbg_stack(mem, vcpufd);
                break;
            case UKVM_PORT_POLL:
                ukvm_port_poll(mem, data);
                break;
            default:
                errx(1, "unhandled KVM_EXIT_IO (%x)", run->io.port);
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
            errx(1, "KVM_EXIT_FAIL_ENTRY: hw_entry_failure_reason = 0x%llx",
                 run->fail_entry.hardware_entry_failure_reason);
        case KVM_EXIT_INTERNAL_ERROR:
            errx(1, "KVM_EXIT_INTERNAL_ERROR: suberror = 0x%x",
                 run->internal.suberror);
        default:
            errx(1, "exit_reason = 0x%x", run->exit_reason);
        }
    }

    return 0; /* XXX Refactor return code paths in the above code */
}

int setup_modules(int vcpufd, uint8_t *mem)
{
    int i;

    for (i = 0; i < NUM_MODULES; i++) {
        if (modules[i]->setup(vcpufd, mem)) {
            printf("Please check you have correctly specified:\n %s\n",
                   modules[i]->usage());
            return -1;
        }
    }
    return 0;
}

void sig_handler(int signo)
{
    printf("Received SIGINT. Exiting\n");
    exit(0);
}

static void usage(const char *prog)
{
    int m;

    printf("usage: %s [ CORE OPTIONS ] [ MODULE OPTIONS ] KERNEL", prog);
    printf(" [ -- ] [ ARGS ]\n");
    printf("Core options:\n");
    printf("    --help (display this help)\n");
    printf("Compiled-in module options:\n");
    for (m = 0; m < NUM_MODULES; m++)
        printf("    %s\n", modules[m]->usage());
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

    if (argc < 1)
        usage(prog);

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
        printf("Invalid option: %s\n", *argv);
        return 1;
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
        err(1, "Can not catch SIGINT");

    kvm = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    if (kvm == -1)
        err(1, "/dev/kvm");

    /* Make sure we have the stable version of the API */
    ret = ioctl(kvm, KVM_GET_API_VERSION, NULL);
    if (ret == -1)
        err(1, "KVM_GET_API_VERSION");
    if (ret != 12)
        errx(1, "KVM_GET_API_VERSION %d, expected 12", ret);

    vmfd = ioctl(kvm, KVM_CREATE_VM, 0);
    if (vmfd == -1)
        err(1, "KVM_CREATE_VM");

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
        err(1, "allocating guest memory");

    load_code(elffile, mem, &elf_entry, &kernel_end);

    struct kvm_userspace_memory_region region = {
        .slot = 0,
        .guest_phys_addr = 0,
        .memory_size = GUEST_SIZE,
        .userspace_addr = (uint64_t) mem,
    };

    ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &region);
    if (ret == -1)
        err(1, "KVM_SET_USER_MEMORY_REGION");


    /* enabling this seems to mess up our receiving of hlt instructions */
    /* ret = ioctl(vmfd, KVM_CREATE_IRQCHIP); */
    /* if (ret == -1) */
    /*     err(1, "KVM_CREATE_IRQCHIP"); */

    vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, 0);
    if (vcpufd == -1)
        err(1, "KVM_CREATE_VCPU");

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
        err(1, "KVM_SET_REGS");


    /* Map the shared kvm_run structure and following data. */
    ret = ioctl(kvm, KVM_GET_VCPU_MMAP_SIZE, NULL);
    if (ret == -1)
        err(1, "KVM_GET_VCPU_MMAP_SIZE");
    mmap_size = ret;
    if (mmap_size < sizeof(*run))
        errx(1, "KVM_GET_VCPU_MMAP_SIZE unexpectedly small");
    run =
        mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpufd,
             0);
    if (run == MAP_FAILED)
        err(1, "mmap vcpu");

    setup_cpuid(kvm, vcpufd);

    /* start event thread */
    pthread_t event_thread;

    ret = pthread_create(&event_thread, NULL, event_loop, (void *) run);
    if (ret)
        err(1, "couldn't create event thread");

    if (setup_modules(vcpufd, mem))
        errx(1, "couldn't setup modules");

    return vcpu_loop(run, vcpufd, mem);
}
