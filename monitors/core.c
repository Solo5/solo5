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
#define _GNU_SOURCE
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <assert.h>
#include <signal.h>
#include <libgen.h>
#include <limits.h>
#include <sys/time.h>

/* from ukvm */
#include "ukvm-elf.h"
#include "ukvm-private.h"
#include "ukvm-modules.h"
#include "ukvm-cpu.h"
#include "ukvm.h"
#include "unikernel-monitor.h"

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
#define BOOT_GDT_CODE32  2
#define BOOT_GDT_DATA    3
#define BOOT_GDT_TSS1    4
#define BOOT_GDT_TSS2    5
#define BOOT_GDT_MAX     6

static uint64_t sleep_time_s;  /* track unikernel sleeping time */
static uint64_t sleep_time_ns;
static uint64_t tsc_freq;

static void setup_boot_info(uint8_t *mem,
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

        daddr = mem + paddr;
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


static void setup_system_64bit(struct platform *p)
{
    uint64_t cr0 = (X86_CR0_NE | X86_CR0_PE | X86_CR0_PG)
        & ~(X86_CR0_NW | X86_CR0_CD);
    uint64_t cr4 = X86_CR4_PAE | X86_CR4_VMXE;
    uint64_t efer = X86_EFER_LME | X86_EFER_LMA;

    /* enable sse */
    cr0 = (cr0 | X86_CR0_MP) & ~(X86_CR0_EM);
    cr4 = cr4 | X86_CR4_FXSR | X86_CR4_XMM; /* OSFXSR and OSXMMEXCPT */

    platform_setup_system_64bit(p, cr0, cr4, efer);
}


static void setup_system_page_tables(struct platform *p)
{
    uint64_t *pml4 = (uint64_t *) (p->mem + BOOT_PML4);
    uint64_t *pdpte = (uint64_t *) (p->mem + BOOT_PDPTE);
    uint64_t *pde = (uint64_t *) (p->mem + BOOT_PDE);
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

    platform_setup_system_page_tables(p, BOOT_PML4);
}

static void setup_system_gdt(struct platform *p, uint64_t off)
{
	uint64_t *gdt_entry;

    gdt_entry = ((uint64_t *) (p->mem + off));
	gdt_entry[0] = 0x0000000000000000;
    gdt_entry[1] = 0x00af9b000000ffff;	/* 64bit CS		*/
    gdt_entry[2] = 0x00cf9b000000ffff;	/* 32bit CS		*/
    gdt_entry[3] = 0x00cf93000000ffff;	/* DS			*/
	gdt_entry[4] = 0x0000000000000000;	/* TSS part 1 (via C)	*/
	gdt_entry[5] = 0x0000000000000000;	/* TSS part 2 (via C)	*/

    platform_setup_system_gdt(p, BOOT_GDT_CODE, BOOT_GDT_DATA,
                              off, (sizeof(uint64_t) * BOOT_GDT_MAX) - 1);
}

static void setup_system(struct platform *p, uint64_t entry)
{
    setup_system_gdt(p, BOOT_GDT);
    setup_system_page_tables(p);
    setup_system_64bit(p);

    platform_setup_system(p, entry, BOOT_INFO);
}

void ukvm_port_puts(uint8_t *mem, uint64_t paddr)
{
    GUEST_CHECK_PADDR(paddr, GUEST_SIZE, sizeof (struct ukvm_puts));
    struct ukvm_puts *p = (struct ukvm_puts *)(mem + paddr);

    GUEST_CHECK_PADDR(p->data, GUEST_SIZE, p->len);
    assert(write(1, mem + p->data, p->len) != -1);
}

static void ukvm_port_time_init(uint8_t *mem, uint64_t paddr)
{
    GUEST_CHECK_PADDR(paddr, GUEST_SIZE, sizeof (struct ukvm_time_init));
    struct ukvm_time_init *p = (struct ukvm_time_init *) (mem + paddr);
    struct timeval tv;
    int ret;

    p->freq = tsc_freq;
    ret = gettimeofday(&tv, NULL);
    assert(ret == 0);
    /* get ns since epoch */
    p->rtc_boot = (((uint64_t)tv.tv_sec * 1000000)
                   + (uint64_t)tv.tv_usec) * 1000;
}

static void ukvm_port_poll(uint8_t *mem, uint64_t paddr)
{
    GUEST_CHECK_PADDR(paddr, GUEST_SIZE, sizeof (struct ukvm_poll));
    struct ukvm_poll *t = (struct ukvm_poll *)(mem + paddr);
    uint64_t ts_s1, ts_ns1, ts_s2, ts_ns2;

    struct timespec ts;
    int rc, i, max_fd = 0;
    fd_set readfds;

    platform_get_timestamp(&ts_s1, &ts_ns1);

    FD_ZERO(&readfds);
    for (i = 0; i < NUM_MODULES; i++) {
        int fd = modules[i]->get_fd();

        if (fd) {
            FD_SET(fd, &readfds);
            if (fd > max_fd)
                max_fd = fd;
        }
    }

    ts.tv_sec = t->timeout_nsecs / 1000000000ULL;
    ts.tv_nsec = t->timeout_nsecs % 1000000000ULL;

    /*
     * Guest execution is blocked during the pselect() call, note that
     * interrupts will not be injected.
     */
    do {
        rc = pselect(max_fd + 1, &readfds, NULL, NULL, &ts, NULL);
    } while (rc == -1 && errno == EINTR);
    assert(rc >= 0);

    platform_get_timestamp(&ts_s2, &ts_ns2);
    sleep_time_s += ts_s2 - ts_s1;
    sleep_time_ns += ts_ns2 - ts_ns1;

    t->ret = rc;
}

static void tsc_init(void)
{
    platform_init_time(&tsc_freq);
    printf("tsc_freq=0x%" PRIx64 "(%" PRIu64 ")\n",
           tsc_freq, tsc_freq);
}


static int vcpu_loop(struct platform *p)
{
    tsc_init();

    /* Repeatedly run code and handle VM exits. */
    while (1) {
        int i, handled = 0;

        if (platform_run(p))
            err(1, "Couldn't run vcpu");

        for (i = 0; i < NUM_MODULES; i++) {
            if (!modules[i]->handle_exit(p)) {
                handled = 1;
                break;
            }
        }

        if (handled)
            continue;

        switch (platform_get_exit_reason(p)) {
        case EXIT_HLT:
            /* Guest has halted the CPU, this is considered as a normal exit. */
            return 0;

        case EXIT_IO: {
            int port = platform_get_io_port(p);
            uint64_t paddr = platform_get_io_data(p);

            switch (port) {
            case UKVM_PORT_PUTS:
                ukvm_port_puts(p->mem, paddr);
                break;
            case UKVM_PORT_POLL:
                ukvm_port_poll(p->mem, paddr);
                break;
            case UKVM_PORT_TIME_INIT:
                ukvm_port_time_init(p->mem, paddr);
                break;
            default:
                errx(1, "Invalid guest port access: port=0x%x", port);
            };

            platform_advance_rip(p);

            break;
        }

        case EXIT_RDTSC: {
            uint64_t exec_time;
            uint64_t sleep_time;
            uint64_t new_tsc;
            double tsc_f;
            int dbg_sanity_check_rdtsc = 0;

            exec_time = platform_get_exec_time(p);

            if (dbg_sanity_check_rdtsc) {
                static uint64_t last_exec_time;
                assert(exec_time > last_exec_time);
                last_exec_time = exec_time;
            }

            sleep_time = ((sleep_time_s * 1000000000ULL) + sleep_time_ns);

            if (dbg_sanity_check_rdtsc) {
                static uint64_t last_sleep_time;
                assert(sleep_time >= last_sleep_time);
                last_sleep_time = sleep_time;
            }

            tsc_f = (((double)exec_time + (double)sleep_time)
                     * (double)tsc_freq) / 1000000000ULL;

            new_tsc = (uint64_t)tsc_f;

            if (dbg_sanity_check_rdtsc) {
                static uint64_t last_tsc;
                assert(new_tsc > last_tsc);
                last_tsc = new_tsc;
            }

            platform_emul_rdtsc(p, new_tsc);
            platform_advance_rip(p);
            break;
        }

        case EXIT_IGNORE:
            break;

        case EXIT_FAIL:
            return -1;

        default:
            errx(1, "Unhandled exit");
        }
    }
}

int setup_modules(struct platform *p)
{
    int i;

    for (i = 0; i < NUM_MODULES; i++) {
        if (modules[i]->setup(p)) {
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
    errx(1, "Exiting on signal %d", signo);
}

static void usage(const char *prog)
{
    int m;

    fprintf(stderr, "usage: %s [ CORE OPTIONS ] [ MODULE OPTIONS ] [ -- ] "
            "KERNEL [ ARGS ]\n", prog);
    fprintf(stderr, "KERNEL is the filename of the unikernel to run.\n");
    fprintf(stderr, "ARGS are optional arguments passed to the unikernel.\n");
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
    struct platform *p;
    uint64_t elf_entry;
    uint64_t kernel_end;
    const char *prog;
    const char *elffile;
    struct sigaction sa;
    int matched;
    int rc;

    prog = basename(*argv);
    argc--;
    argv++;

    while (*argv && *argv[0] == '-') {
        int j;

        if (strcmp("--help", *argv) == 0)
            usage(prog);

        if (strcmp("--", *argv) == 0) {
            /* Consume and stop arg processing */
            argc--;
            argv++;
            break;
        }

        matched = 0;
        for (j = 0; j < NUM_MODULES; j++) {
            if (modules[j]->handle_cmdarg(*argv) == 0) {
                /* Handled by module, consume and go on to next arg */
                matched = 1;
                argc--;
                argv++;
                break;
            }
        }
        if (!matched) {
            warnx("Invalid option: `%s'", *argv);
            usage(prog);
        }
    }

    /* At least one non-option argument required */
    if (*argv == NULL) {
        warnx("Missing KERNEL operand");
        usage(prog);
    }
    elffile = *argv;
    argc--;
    argv++;

    memset(&sa, 0, sizeof (struct sigaction));
    sa.sa_handler = sig_handler;
    sigfillset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1)
        err(1, "Could not install signal handler");
    if (sigaction(SIGTERM, &sa, NULL) == -1)
        err(1, "Could not install signal handler");

    if (platform_init(&p))
        err(1, "platform init");

    load_code(elffile, p->mem, &elf_entry, &kernel_end);

    /* Setup x86 registers and memory */
    setup_system(p, elf_entry);
    /* Setup ukvm_boot_info and command line */
    setup_boot_info(p->mem, GUEST_SIZE, kernel_end, argc, argv);

    if (setup_modules(p))
        exit(1);

    rc = vcpu_loop(p);
    platform_cleanup(p);
	return rc;
}
