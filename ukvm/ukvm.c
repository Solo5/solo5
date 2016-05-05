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

#include <err.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* for net */
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <linux/if_tun.h>


#include "processor-flags.h"
#include "../kernel/interrupts.h"
#include "ukvm.h"
#include "misc.h"


struct ukvm_blkinfo blkinfo;


/*           _ loaded elf file (linker script dictates location) */
/* 0x100000 | */
/* 0x09fc00 |_ memory hole */
/* ########    <-- 0x94c00 unused */
/* 0x00B000  _ */
/* 0x00A000 |_ PDPTE entries (present, rw, 1GB page size, points to 0) */
/* 0x009000 |_ PML4 entry (present, rw, points to PDPTE) */
/* ########    <-- 0x200 unused */
/* 0x008e00  _ */
/* 0x008000 |  <-- bootstrap stack (yikes: will destroy boot info) */
/* 0x007e00 |_ boot info (1 page) */
/* ########    <-- 0x78d8 unused */
/* 0x000528  _ */
/* 0x000520 |_ idt (is blank)  (yikes: overlaps 2nd half of tss entry in gdt) */
/* 0x000500 |_ gdt (contains correct code/data/ but tss points to 0) */


/*           _ loaded elf file (linker script dictates location) 
 * 0x100000 | 
 * 0x09fc00 |_ memory hole 
 * ########    <-- 0x94c00 unused 
 * 0x00B000  _ 
 * 0x00A000 |_ PDPTE entries (present, rw, 1GB page size, points to 0) 
 * 0x009000 |_ PML4 entry (present, rw, points to PDPTE) 
 * ########    <-- 0x200 unused 
 * 0x008e00  _ 
 * 0x008000 |  <-- bootstrap stack (yikes: will destroy boot info) 
 * 0x007e00 |_ boot info (1 page) 
 * ########  _ <--  unused 
 * 0x003000 |  space for interrupt stack
 * 0x002067 |_
 * 0x002000 |_ tss structure
 * 0x001000 |_ idt 
 * 0x000500 |_ gdt (contains correct code/data/ but tss points to 0) 
 */


#define GUEST_PAGE_SIZE 0x200000   // 2 MB pages in guest

#define BOOT_IST    0x3000
#define BOOT_TSS    0x2000
#define BOOT_IDT    0x1000
#define BOOT_GDT    0x500

#define IDT_SIZE    0x1000

/* Puts PML4 right after zero page but aligned to 4k */
#define BOOT_PML4    0x9000
#define BOOT_PDPTE   0xA000
#define BOOT_PDE     0xB000

#define BOOT_GDT_NULL    0
#define BOOT_GDT_CODE    1
#define BOOT_GDT_DATA    2
#define BOOT_GDT_TSS     3
#define BOOT_GDT_TSS2    4
#define BOOT_GDT_MAX     5
#define GDT_DESC_OFFSET(n) ((n) * 0x8)

#define GDT_GET_BASE(x)                         \
    (( (x) & 0xFF00000000000000) >> 32) |       \
    (( (x) & 0x000000FF00000000) >> 16) |       \
    (( (x) & 0x00000000FFFF0000) >> 16)

#define GDT_GET_LIMIT(x) (__u32)(                                       \
                                 (( (x) & 0x000F000000000000) >> 32) |  \
                                 (( (x) & 0x000000000000FFFF)))

/* Constructor for a conventional segment GDT (or LDT) entry */
/* This is a macro so it can be used in initializers */
#define GDT_ENTRY(flags, base, limit)               \
    ((((base)  & _AC(0xff000000,ULL)) << (56-24)) | \
     (((flags) & _AC(0x0000f0ff,ULL)) << 40) |      \
     (((limit) & _AC(0x000f0000,ULL)) << (48-16)) | \
     (((base)  & _AC(0x00ffffff,ULL)) << 16) |      \
     (((limit) & _AC(0x0000ffff,ULL))))


struct __attribute__ ((__packed__)) tss {
    uint32_t reserved;
    uint32_t rsp0_lo;
    uint32_t rsp0_hi;
    uint32_t rsp1_lo;
    uint32_t rsp1_hi;
    uint32_t rsp2_lo;
    uint32_t rsp2_hi;
    uint32_t reserved2;
    uint32_t reserved3;
    uint32_t ist1_lo;
    uint32_t ist1_hi;
    uint32_t ist2_lo;
    uint32_t ist2_hi;
    uint32_t ist3_lo;
    uint32_t ist3_hi;
    uint32_t ist4_lo;
    uint32_t ist4_hi;
    uint32_t ist5_lo;
    uint32_t ist5_hi;
    uint32_t ist6_lo;
    uint32_t ist6_hi;
    uint32_t ist7_lo;
    uint32_t ist7_hi;
    uint32_t reserved4;
    uint32_t reserved5;
    uint16_t reserved6;
    uint16_t iomap_base;
};

struct _kvm_segment {
    __u64 base;
    __u32 limit;
    __u16 selector;
    __u8 type;
    __u8 present, dpl, db, s, l, g, avl;
    __u8 unusable;
    __u8 padding;
};


#define GDT_GET_G(x)   (__u8)(( (x) & 0x0080000000000000) >> 55)
#define GDT_GET_DB(x)  (__u8)(( (x) & 0x0040000000000000) >> 54)
#define GDT_GET_L(x)   (__u8)(( (x) & 0x0020000000000000) >> 53)
#define GDT_GET_AVL(x) (__u8)(( (x) & 0x0010000000000000) >> 52)
#define GDT_GET_P(x)   (__u8)(( (x) & 0x0000800000000000) >> 47)
#define GDT_GET_DPL(x) (__u8)(( (x) & 0x0000600000000000) >> 45)
#define GDT_GET_S(x)   (__u8)(( (x) & 0x0000100000000000) >> 44)
#define GDT_GET_TYPE(x)(__u8)(( (x) & 0x00000F0000000000) >> 40)



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
    } while (0);

#define KVM_32BIT_MAX_MEM_SIZE  (1ULL << 32)
#define KVM_32BIT_GAP_SIZE    (768 << 20)
#define KVM_32BIT_GAP_START    (KVM_32BIT_MAX_MEM_SIZE - KVM_32BIT_GAP_SIZE)

void gdb_stub_start();
void gdb_handle_exception(int vcpufd, int sig);
int gdb_is_pc_breakpointing(long addr);

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
static void load_code(const char *file,	  /* IN */
                      uint8_t * mem,      /* IN */
                      uint64_t * p_entry, /* OUT */
                      uint64_t * p_end    /* OUT */ )
{
    int fd_kernel;
    int ret = 0;
    ssize_t numb;
    size_t buflen;
    Elf64_Off ph_off;
    Elf64_Half ph_entsz;
    Elf64_Half ph_cnt;
    Elf64_Half ph_i;
    Elf64_Phdr *phdr = NULL;
    Elf64_Ehdr hdr;

    // elf entry point (on physical memory)
    *p_entry = 0;
    // highest byte of the program (on physical memory)
    *p_end = 0;

    fd_kernel = open(file, O_RDONLY);
    if (!fd_kernel)
        err(1, "couldn't open elf");

    numb = pread_in_full(fd_kernel, &hdr, sizeof(Elf64_Ehdr), 0);
    if (numb < 0 || (size_t) numb != sizeof(Elf64_Ehdr)) {
        err(1, "unable to read ELF64 hdr");
    }
    ph_off = hdr.e_phoff;
    ph_entsz = hdr.e_phentsize;
    ph_cnt = hdr.e_phnum;
    buflen = ph_entsz * ph_cnt;

    phdr = malloc(buflen);
    if (!phdr) {
        err(1, "unable to allocate program header buffer\n");
    }

    numb = pread_in_full(fd_kernel, phdr, buflen, ph_off);
    if (numb < 0 || (size_t) numb != buflen) {
        err(1, "unable to read program header");
    }

    /*
     * Load all segments with the LOAD directive from the elf file at offset
     * p_offset, and copy that into p_addr in memory. The amount of bytes
     * copied is p_filesz.  However, each segment should be given
     * ALIGN_UP(p_memsz, p_align) bytes on memory.
     */
    for (ph_i = 0; ph_i < ph_cnt; ph_i++) {
        char *dst;
        size_t _end;
        size_t offset = phdr[ph_i].p_offset;
        size_t filesz = phdr[ph_i].p_filesz;
        size_t memsz = phdr[ph_i].p_memsz;
        uint64_t paddr = phdr[ph_i].p_paddr;
        uint64_t align = phdr[ph_i].p_align;

        if ((phdr[ph_i].p_type & PT_LOAD) == 0)
            continue;

        // XXX this won't work after having the two memory slots
        assert(GUEST_SIZE < KVM_32BIT_GAP_SIZE);
        dst = mem + paddr;

        numb = pread_in_full(fd_kernel, dst, filesz, offset);
        if (numb < 0 || (size_t) numb != filesz) {
            err(1, "unable to load segment");
        }

        memset(mem + paddr + filesz, 0, memsz - filesz);

        // Protect the executable code
        if (phdr[ph_i].p_flags & ELF_SEGMENT_X) {
            mprotect((void *) dst, memsz, PROT_EXEC | PROT_READ);
        }

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


static void setup_system_page_tables(struct kvm_sregs *sregs, uint8_t * mem)
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

static void print_dtable(const char *name, struct kvm_dtable *dtable)
{
    printf(" %s                 %016lx  %08hx\n",
           name, (uint64_t) dtable->base, (uint16_t) dtable->limit);
}


static void print_segment(const char *name, struct kvm_segment *seg)
{
    printf
        (" %s       %04hx      %016lx  %08x  %02hhx    %x %x   %x  %x %x %x %x\n",
         name, (uint16_t) seg->selector, (uint64_t) seg->base,
         (uint32_t) seg->limit, (uint8_t) seg->type, seg->present,
         seg->dpl, seg->db, seg->s, seg->l, seg->g, seg->avl);
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
    printf(" register  selector  base              limit     type  p dpl db s l g avl\n");

    print_segment("cs ", &sregs->cs);
    print_segment("ss ", &sregs->ss);
    print_segment("ds ", &sregs->ds);
    print_segment("es ", &sregs->es);
    print_segment("fs ", &sregs->fs);
    print_segment("gs ", &sregs->gs);
    print_segment("tr ", &sregs->tr);
    print_segment("ldt", &sregs->ldt);
    print_dtable("gdt", &sregs->gdt);
    print_dtable("idt", &sregs->idt);
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

static void setup_system_tss(struct kvm_sregs *sregs,
                             uint8_t * mem,
                             uint64_t off)
{
    struct tss *tss = (struct tss *) (mem + off);

    memset(tss, 0, sizeof(struct tss));
    tss->ist1_lo = tss->rsp0_lo = (uint32_t) BOOT_IST;
    tss->ist1_hi = tss->rsp0_hi = 0;
    tss->iomap_base = sizeof(struct tss);
}

static void setup_system_gdt(struct kvm_sregs *sregs,
                             uint8_t *mem,
                             uint64_t off)
{
    uint64_t *gdt = (uint64_t *) (mem + off);
    struct kvm_segment data_seg, code_seg, tss_seg;

    /* flags, base, limit */
    gdt[BOOT_GDT_NULL] = GDT_ENTRY(0, 0, 0);
    gdt[BOOT_GDT_CODE] = GDT_ENTRY(0xA09B, 0, 0xFFFFF);
    gdt[BOOT_GDT_DATA] = GDT_ENTRY(0xC093, 0, 0xFFFFF);
    //gdt[BOOT_GDT_CODE] = GDT_ENTRY(0xA09b, 0, 0xFFFFF);
    //gdt[BOOT_GDT_DATA] = GDT_ENTRY(0x809a, 0, 0xFFFFF);
    //gdt[BOOT_GDT_TSS ] = GDT_ENTRY(0x808B, 0, 0xFFFFF);
    gdt[BOOT_GDT_TSS] = GDT_ENTRY(0x008B, BOOT_TSS, 0x67);
    gdt[BOOT_GDT_TSS2] = GDT_ENTRY(0, 0, 0);

    if (0) {
        size_t i;
        printf("gdt @ 0x%x \n", BOOT_GDT);
        for (i = 0; i < 40; i++) {
            printf("%02x", *((uint8_t *) gdt + i));
            if (i % 8 == 7)
                printf("\n");
        }
        /* printk("tss @ 0x%lx \n", &tss); */
        /* for (i = 0; i < sizeof(tss); i++) { */
        /*     printk("%02x", *((uint8_t *)&tss + i)); */
        /*     if (i % 8 == 7) */
        /*         printk("\n"); */
        /* } */
    }

    sregs->gdt.base = off;
    sregs->gdt.limit = (sizeof(uint64_t) * BOOT_GDT_MAX) - 1;

    GDT_TO_KVM_SEGMENT(code_seg, gdt, BOOT_GDT_CODE);
    GDT_TO_KVM_SEGMENT(data_seg, gdt, BOOT_GDT_DATA);
    GDT_TO_KVM_SEGMENT(tss_seg, gdt, BOOT_GDT_TSS);

    sregs->cs = code_seg;
    sregs->ds = data_seg;
    sregs->es = data_seg;
    sregs->fs = data_seg;
    sregs->gs = data_seg;
    sregs->ss = data_seg;
    sregs->tr = tss_seg;
}

/* the interrupt stack is the first entry in the TSS */
#define IDT_IST 0x1
#define IDT_TYPE_INT64  0xe00
#define IDT_TYPE_TRAP64 0xf00
#define IDT_PRESENT 0x8000
#define IDT_HANDLER_SEGMENT (GDT_DESC_OFFSET(BOOT_GDT_CODE))
#define SET_IDT_ENTRY(base, n, a) do {                                  \
        uint32_t *entry = (uint32_t *)((uint64_t)base + n * (sizeof(uint32_t) * 4)); \
        *entry++ = (IDT_HANDLER_SEGMENT << 16) | (a & 0xffff);          \
        *entry++ = (a & 0xffff0000)                                     \
            | IDT_PRESENT | IDT_TYPE_INT64 | IDT_IST;                   \
        *entry++ = a >> 32;                                             \
        *entry = 0;                                                     \
    } while(0)

static void set_idt_entry(uint64_t base, int n, uint64_t addr)
{
    uint32_t *entry = (uint32_t *) (base + (n * (sizeof(uint32_t) * 4)));
    *entry++ = (IDT_HANDLER_SEGMENT << 16) | (addr & 0xffff);
    *entry++ =
        (addr & 0xffff0000) | IDT_PRESENT | IDT_TYPE_INT64 | IDT_IST;
    *entry++ = addr >> 32;
    *entry = 0;
}

static void setup_system_idt(struct kvm_sregs *sregs,
                             uint8_t *mem,
                             uint64_t off)
{
    uint64_t *idt = (uint64_t *) (mem + off);
    memset(idt, 0, IDT_SIZE);
#if 1
    /* hardcoded where divide-by-zero is */
    // SET_IDT_ENTRY((uint64_t)idt, 0, 0x10a30c);
    /* hardcoded where divide-by-zero is */
    // set_idt_entry((uint64_t)idt, 0, 0x10a32c);
    //
    /* hardcoded where divide-by-zero is */
    set_idt_entry((uint64_t) idt, 0, 0x1b3c89);
    if (0) {
        int i;
        printf("setting IDT: ");
        for (i = 0; i < 16; i++)
            printf("%02x", mem[off + i]);
        printf("\n");
    }
#else
    /* set up memory */
    *idt = 0;
#endif

    /* set up cpu */
    sregs->idt.base = off;
    sregs->idt.limit = IDT_SIZE - 1;
}

static void setup_system(int vcpufd, uint8_t * mem)
{
    struct kvm_sregs sregs;
    int ret;

    /* Set all cpu/mem system structures */
    ret = ioctl(vcpufd, KVM_GET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM_GET_SREGS");

    setup_system_gdt(&sregs, mem, BOOT_GDT);
    setup_system_idt(&sregs, mem, BOOT_IDT);
    setup_system_page_tables(&sregs, mem);
    setup_system_64bit(&sregs);

    //dump_sregs(&sregs);

    ret = ioctl(vcpufd, KVM_SET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM_SET_SREGS");
}


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
}


void ukvm_port_puts(uint8_t * mem, void *data)
{
    uint32_t mem_off = *(uint32_t *) data;
    struct ukvm_puts *p = (struct ukvm_puts *) (mem + mem_off);

    printf("%.*s", p->len, (char *) (mem + (uint64_t) p->data));
    //putchar(*(char *)data);
}


void ukvm_port_nanosleep(uint8_t * mem, void *data, struct kvm_run *run)
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


void ukvm_port_clkspeed(uint8_t * mem, void *data)
{
    uint32_t mem_off = *(uint32_t *) data;
    struct ukvm_clkspeed *c = (struct ukvm_clkspeed *) (mem + mem_off);

    /* Horrible hack to get cpu speed */
    FILE *cpuinfo;
    cpuinfo =
        popen
        ("cat /proc/cpuinfo | grep MHz | head -n 1 | "
         "awk '{print $4}'| sed s/\\\\.//|sed s/$/000/",
         "r");
    c->clkspeed = 0;
    fscanf(cpuinfo, "%ld", &c->clkspeed);
    pclose(cpuinfo);
}

void ukvm_port_blkinfo(uint8_t * mem, void *data)
{
    uint32_t mem_off = *(uint32_t *) data;
    struct ukvm_blkinfo *info = (struct ukvm_blkinfo *) (mem + mem_off);

    info->sector_size = blkinfo.sector_size;
    info->num_sectors = blkinfo.num_sectors;
    info->rw = blkinfo.rw;
}


void ukvm_port_blkwrite(uint8_t * mem, void *data, int diskfd)
{
    uint32_t mem_off = *(uint32_t *) data;
    struct ukvm_blkwrite *wr = (struct ukvm_blkwrite *) (mem + mem_off);
    int ret;

    wr->ret = -1;
    if (wr->sector < blkinfo.num_sectors) {
        lseek(diskfd, blkinfo.sector_size * wr->sector, SEEK_SET);
        ret = write(diskfd, mem + (uint64_t) wr->data, wr->len);
        assert(ret == wr->len);
        wr->ret = 0;
    }
}


void ukvm_port_blkread(uint8_t * mem, void *data, int diskfd)
{
    uint32_t mem_off = *(uint32_t *) data;
    struct ukvm_blkread *rd = (struct ukvm_blkread *) (mem + mem_off);
    int ret;

    rd->ret = -1;
    if (rd->sector < blkinfo.num_sectors) {
        lseek(diskfd, blkinfo.sector_size * rd->sector, SEEK_SET);
        ret = read(diskfd, mem + (uint64_t) rd->data, rd->len);
        assert(ret == rd->len);
        rd->ret = 0;
    }
}


void ukvm_port_netinfo(uint8_t * mem, void *data)
{
    uint32_t mem_off = *(uint32_t *) data;
    struct ukvm_netinfo *info = (struct ukvm_netinfo *) (mem + mem_off);

    printf("%s: WARNING: returning hardcoded MAC\n", __FILE__);
    strcpy(info->mac_str, "52:54:00:12:34:56");
}

void ukvm_port_netwrite(uint8_t * mem, void *data, int netfd)
{
    uint32_t mem_off = *(uint32_t *) data;
    struct ukvm_netwrite *wr = (struct ukvm_netwrite *) (mem + mem_off);
    uint8_t *ptr = mem + (uint64_t) wr->data;
    int i;
    int ret;
    
    wr->ret = 0;
    ret = write(netfd, mem + (uint64_t) wr->data, wr->len);
    assert(wr->len == ret);
}


void ukvm_port_netread(uint8_t * mem, void *data, int netfd)
{
    uint32_t mem_off = *(uint32_t *) data;
    struct ukvm_netread *rd = (struct ukvm_netread *) (mem + mem_off);
    uint8_t *ptr = mem + (uint64_t) rd->data;
    struct timeval zero;
    fd_set netset;
    int ret;

    FD_ZERO(&netset);
    FD_SET(netfd, &netset);
    zero.tv_sec = 0;
    zero.tv_usec = 0;
    ret = select(netfd + 1, &netset, NULL, NULL, &zero);
    if (ret <= 0) {
        rd->ret = -1;
        return;
    }

    rd->len = read(netfd, mem + (uint64_t) rd->data, rd->len);
    rd->ret = 0;
}


void ukvm_port_dbg_stack(uint8_t *mem, int vcpufd){
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
        i -=8;
    }
}

static int vcpu_loop(struct kvm_run *run, int vcpufd, uint8_t *mem,
                     int diskfd, int netfd)
{
    int ret;

    /* Repeatedly run code and handle VM exits. */
    while (1) {
        ret = ioctl(vcpufd, KVM_RUN, NULL);

        switch (run->exit_reason) {
        case KVM_EXIT_DEBUG: {
            struct kvm_debug_exit_arch *arch_info = &run->debug.arch;
            if (gdb_is_pc_breakpointing(arch_info->pc))
                gdb_handle_exception(vcpufd, 1);
            break;
        }
        case KVM_EXIT_HLT: {
            puts("KVM_EXIT_HLT");
            //get_and_dump_sregs(vcpufd);
            return 0;
        }
        case KVM_EXIT_IO: {
            uint8_t *data = (uint8_t *)run + run->io.data_offset;

            assert( run->io.direction == KVM_EXIT_IO_OUT );
    
            switch (run->io.port) {
            case UKVM_PORT_PUTS:
                ukvm_port_puts(mem, data);
                break;
            case UKVM_PORT_NANOSLEEP:
                ukvm_port_nanosleep(mem, data, run);
                break;
            case UKVM_PORT_CLKSPEED:
                ukvm_port_clkspeed(mem, data);
                break;
            case UKVM_PORT_BLKINFO:
                ukvm_port_blkinfo(mem, data);
                break;
            case UKVM_PORT_BLKWRITE:
                ukvm_port_blkwrite(mem, data, diskfd);
                break;
            case UKVM_PORT_BLKREAD:
                ukvm_port_blkread(mem, data, diskfd);
                break;
            case UKVM_PORT_NETINFO:
                ukvm_port_netinfo(mem, data);
                break;
            case UKVM_PORT_NETWRITE:
                ukvm_port_netwrite(mem, data, netfd);
                break;
            case UKVM_PORT_NETREAD:
                ukvm_port_netread(mem, data, netfd);
                break;
            case UKVM_PORT_DBG_STACK:
                ukvm_port_dbg_stack(mem, vcpufd);
                break;
            default:
                errx(1, "unhandled KVM_EXIT_IO (%x)", run->io.port);
            };
            break;
        }
        case KVM_EXIT_IRQ_WINDOW_OPEN: {
            run->request_interrupt_window = 0;
            inject_interrupt(vcpufd, INTR_TIMER);
            //inject_interrupt(vcpufd, 0x31);
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
            errx(1, "KVM_EXIT_FAIL_ENTRY: hardware_entry_failure_reason = 0x%llx",
                 (unsigned long long)run->fail_entry.hardware_entry_failure_reason);
        case KVM_EXIT_INTERNAL_ERROR:
            errx(1, "KVM_EXIT_INTERNAL_ERROR: suberror = 0x%x", run->internal.suberror);
        default:
            errx(1, "exit_reason = 0x%x", run->exit_reason);
        }
    }
}


/*
 * Create or reuse a TUN or TAP device named 'dev'.
 *
 * Copied from kernel docs: Documentation/networking/tuntap.txt
 */
int tun_alloc(char *dev, int flags)
{
    struct ifreq ifr;
    int fd, err;
    char *clonedev = "/dev/net/tun";

    /* Arguments taken by the function:
     *
     * char *dev: the name of an interface (or '\0'). MUST have enough
     *   space to hold the interface name if '\0' is passed
     * int flags: interface flags (eg, IFF_TUN etc.)
     */

    /* open the clone device */
    if ((fd = open(clonedev, O_RDWR)) < 0) {
        return fd;
    }

    /* preparation of the struct ifr, of type "struct ifreq" */
    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = flags;	/* IFF_TUN or IFF_TAP, plus maybe IFF_NO_PI */

    if (*dev) {
        /* if a device name was specified, put it in the structure; otherwise,
         * the kernel will try to allocate the "next" device of the
         * specified type */
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }

    /* try to create the device */
    if ((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0) {
        close(fd);
        return err;
    }

    /* if the operation was successful, write back the name of the
     * interface to the variable "dev", so the caller can know
     * it. Note that the caller MUST reserve space in *dev (see calling
     * code below) */
    strcpy(dev, ifr.ifr_name);

    /* this is the special file descriptor that the caller will use to talk
     * with the virtual interface */
    return fd;
}

uint8_t *mem;

int main(int argc, char **argv)
{
    int kvm, vmfd, vcpufd, diskfd, netfd, ret;
    uint8_t *tss_addr;
    struct kvm_sregs sregs;
    struct kvm_run *run;
    size_t mmap_size;
    uint64_t elf_entry;
    uint64_t kernel_end;
    char tun_name[IFNAMSIZ];
    int use_gdb = 0;

    if (argc < 4)
        err(1, "usage: ukvm <elf> <disk.img> <net_iface> [--gdb]");

    const char *elffile = argv[1];
    const char *diskfile = argv[2];
    const char *netiface = argv[3];
    if (argc >= 5)
        use_gdb = strcmp(argv[4], "--gdb") == 0;

    /* set up virtual disk */
    diskfd = open(diskfile, O_RDWR);
    if (diskfd == -1)
        err(1, "couldn't open disk");

    blkinfo.sector_size = 512;
    blkinfo.num_sectors = lseek(diskfd, 0, SEEK_END) / 512;
    blkinfo.rw = 1;

    printf("Providing disk: %ld sectors @ %d = %ld bytes\n",
           blkinfo.num_sectors, blkinfo.sector_size,
           blkinfo.num_sectors * blkinfo.sector_size);

    /* set up virtual network */
    strcpy(tun_name, netiface);
    netfd = tun_alloc(tun_name, IFF_TAP | IFF_NO_PI);	/* TAP interface */
    if (netfd < 0) {
        perror("Allocating interface");
        exit(1);
    }

    kvm = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    if (kvm == -1)
        err(1, "/dev/kvm");

    /* Make sure we have the stable version of the API */
    ret = ioctl(kvm, KVM_GET_API_VERSION, NULL);
    if (ret == -1)
        err(1, "KVM_GET_API_VERSION");
    if (ret != 12)
        errx(1, "KVM_GET_API_VERSION %d, expected 12", ret);

    vmfd = ioctl(kvm, KVM_CREATE_VM, (unsigned long) 0);
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

    /* needed for intel processors to hold the ept identity table? */
    /* ret = ioctl(vmfd, KVM_SET_TSS_ADDR, 0xfffbd000); */
    /* if (ret == -1) */
    /*     err(1, "KVM_SET_TSS_ADDR"); */

    vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, (unsigned long) 0);
    if (vcpufd == -1)
        err(1, "KVM_CREATE_VCPU");


    /* Setup x86 system registers and memory. */
    setup_system(vcpufd, mem);

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
        .rsp = GUEST_SIZE,
        .rdi = GUEST_SIZE,	// size arg in kernel main
        .rsi = kernel_end,	// kernel_end arg in kernel main
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


    /* start event thread */
    pthread_t event_thread;
    ret = pthread_create(&event_thread, NULL, event_loop, (void *) run);
    if (ret)
        err(1, "couldn't create event thread");

    if (use_gdb) {
        // TODO check if we have the KVM_CAP_SET_GUEST_DEBUG capbility
        struct kvm_guest_debug debug = {
            .control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_SINGLESTEP,
        };

        if (ioctl(vcpufd, KVM_SET_GUEST_DEBUG, &debug) < 0)
            printf("KVM_SET_GUEST_DEBUG failed");

        gdb_stub_start(vcpufd);
    }

    return vcpu_loop(run, vcpufd, mem, diskfd, netfd);
}
