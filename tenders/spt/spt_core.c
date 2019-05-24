/*
 * Copyright (c) 2015-2019 Contributors as noted in the AUTHORS file
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

/*
 * spt_core.c: Core functionality.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <libgen.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <seccomp.h>
#include <sys/personality.h>

#if defined(__x86_64__)
#include <asm/prctl.h>
#endif

#include "spt.h"

/*
 * TODO: This is copied from 'hvt' in a rather quick and dirty fashion to get
 * a working "net" and "block"; needs a re-think.
 */

struct spt_module spt_module_core;

struct spt_module *spt_core_modules[] = {
    &spt_module_core,
    &spt_module_net,
    &spt_module_block,
    NULL,
};
#define NUM_MODULES ((sizeof spt_core_modules / sizeof (struct spt_module *)) - 1)

/*
 * TODO: Split up the functions in this module better, and introduce something
 * similar to hvt_gpa_t for clarity.
 */

/*
 * Defined by standard GNU ld linker scripts to the lowest address of the text
 * segment.
 */
extern long __executable_start;

static bool use_exec_heap = false;

struct spt *spt_init(size_t mem_size)
{
    struct spt *spt = malloc(sizeof (struct spt));
    if (spt == NULL)
        err(1, "malloc");
    memset(spt, 0, sizeof (struct spt));

#if defined(__PIE__)
    /*
     * On systems where we are built as a PIE executable:
     *
     * The kernel will apply ASLR and map the tender at a high virtual address
     * (see ELF_ET_DYN_BASE in the kernel source for the arch-specific value,
     * as we only support 64-bit architectures for now where this should always
     * be >= 4 GB).
     *
     * Therefore, rather than mislead the user with an incorrect error message,
     * assert that a) the tender has been loaded with a base address of at
     * least 4GB and b) tender address space does not overlap with guest
     * address space. We can re-visit this if it turns out that users run on
     * systems where this does not hold (e.g. kernel ASLR is disabled).
     */
    assert((uint64_t)&__executable_start >= (1ULL << 32));
    assert((uint64_t)(mem_size - 1) < (uint64_t)&__executable_start);
#else
    /*
     * On systems where we are NOT built as a PIE executable, first assert that
     * -Ttext-segment has been correctly passed at the link step (see
     * configure.sh), and then check that guest memory size is within limits.
     */
    assert((uint64_t)&__executable_start >= (1ULL << 30));
    if ((uint64_t)(mem_size - 1) >= (uint64_t)&__executable_start) {
        uint64_t max_mem_size_mb = (uint64_t)&__executable_start >> 20;
        warnx("Maximum guest memory size (%lu MB) exceeded.",
                max_mem_size_mb);
        errx(1, "Either decrease --mem-size, or recompile solo5-spt"
                " as a PIE executable.");
    }
#endif

    /*
     * Sooo... it turns out that at least on some distributions, the Linux
     * "personality" flag READ_IMPLIES_EXEC is the default unless linked with
     * -z noexecstack. This is bad, as it results in mmap() with PROT_READ
     *  implying PROT_EXEC. Cowardly refuse to run on such systems.
     */
    int persona = -1;
    persona = personality(0xffffffff);
    assert(persona >= 0);
    if (persona & READ_IMPLIES_EXEC)
        errx(1, "Cowardly refusing to run with a sys_personality of "
                "READ_IMPLIES_EXEC. Please report a bug, with details of your "
                "Linux distribution and GCC version");

    /*
     * spt->mem is addressed starting at 0, however we cannot actually map it
     * at 0 due to restrictions on mapping low memory addresses present in
     * modern Linux kernels (vm.mmap_min_addr sysctl). Therefore, we map
     * spt_mem at SPT_HOST_MEM_BASE, adjusting the returned pointer and region
     * size appropriately.
     */
    int prot = PROT_READ | PROT_WRITE | (use_exec_heap ? PROT_EXEC : 0);
    spt->mem = mmap((void *)SPT_HOST_MEM_BASE, mem_size - SPT_HOST_MEM_BASE,
            prot, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (spt->mem == MAP_FAILED)
        err(1, "Error allocating guest memory");
    assert(spt->mem == (void *)SPT_HOST_MEM_BASE);
    spt->mem -= SPT_HOST_MEM_BASE;
    spt->mem_size = mem_size;

    spt->sc_ctx = seccomp_init(SCMP_ACT_KILL);
    assert(spt->sc_ctx != NULL);

    return spt;
}

void spt_bi_init(struct spt *spt, uint64_t p_end, char **cmdline)
{
    spt->bi = (struct spt_boot_info *)(spt->mem + SPT_BOOT_INFO_BASE);
    memset(spt->bi, 0, sizeof (struct spt_boot_info));
    spt->bi->cmdline = (char *)spt->mem + SPT_CMDLINE_BASE;
    spt->bi->mem_size = spt->mem_size;
    spt->bi->kernel_end = p_end;

    *cmdline = (char *)spt->mem + SPT_CMDLINE_BASE;
}

/*
 * Defined in spt_lauch_<arch>.S.
 */
extern void spt_launch(uint64_t stack_start, void (*fn)(void *), void *arg);

void spt_run(struct spt *spt, uint64_t p_entry)
{
    typedef void (*start_fn_t)(void *arg);
    start_fn_t start_fn = (start_fn_t)(spt->mem + p_entry);
    /*
     * Set initial stack alignment based on arch-specific ABI requirements.
     */
#if defined(__x86_64__)
    uint64_t sp = spt->mem_size - 0x8;
#elif defined(__aarch64__)
    uint64_t sp = spt->mem_size - 0x10;
#elif defined(__powerpc64__)
    uint64_t sp = spt->mem_size - 0x10;
#else
#error Unsupported architecture
#endif

    int rc = -1;
    rc = seccomp_load(spt->sc_ctx);
    if (rc != 0)
        errx(1, "seccomp_load() failed: %s", strerror(-rc));

    spt_launch(sp, start_fn, spt->mem + SPT_BOOT_INFO_BASE);

    abort(); /* spt_launch() does not return */
}

static int handle_cmdarg(char *cmdarg)
{
    if (!strncmp("--x-exec-heap", cmdarg, 13)) {
	warnx("WARNING: The use of --x-exec-heap is dangerous and not"
              " recommended as it makes the heap and stack executable.");
        use_exec_heap = true;
        return 0;
    }
    return -1;
}

static int setup(struct spt *spt)
{
    int rc = -1;

    rc = seccomp_rule_add(spt->sc_ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 1,
            SCMP_A0(SCMP_CMP_EQ, 1));
    if (rc != 0)
        errx(1, "seccomp_rule_add(write, fd=1) failed: %s", strerror(-rc));
    rc = seccomp_rule_add(spt->sc_ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0);
    if (rc != 0)
        errx(1, "seccomp_rule_add(exit_group) failed: %s", strerror(-rc));
    rc = seccomp_rule_add(spt->sc_ctx, SCMP_ACT_ALLOW, SCMP_SYS(ppoll), 1,
            SCMP_A3(SCMP_CMP_EQ, 0));
    if (rc != 0)
        errx(1, "seccomp_rule_add(ppoll) failed: %s", strerror(-rc));
    rc = seccomp_rule_add(spt->sc_ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_gettime),
            1, SCMP_A0(SCMP_CMP_EQ, CLOCK_MONOTONIC));
    if (rc != 0)
        errx(1, "seccomp_rule_add(clock_gettime, CLOCK_MONOTONIC) failed: %s",
                strerror(-rc));
    rc = seccomp_rule_add(spt->sc_ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_gettime),
            1, SCMP_A0(SCMP_CMP_EQ, CLOCK_REALTIME));
    if (rc != 0)
        errx(1, "seccomp_rule_add(clock_gettime, CLOCK_REALTIME) failed: %s",
                strerror(-rc));
#if defined(__x86_64__)
    rc = seccomp_rule_add(spt->sc_ctx, SCMP_ACT_ALLOW, SCMP_SYS(arch_prctl),
            1, SCMP_A0(SCMP_CMP_EQ, ARCH_SET_FS));
    if (rc != 0)
        errx(1, "seccomp_rule_add(arch_prctl, ARCH_SET_FS) failed: %s",
                strerror(-rc));
#endif

    return 0;
}

static char *usage(void)
{
    return "--x-exec-heap (make the heap executable)."
	   " WARNING: This option is dangerous and not recommended as it"
           " makes the heap and stack executable.";
}

struct spt_module spt_module_core = {
    .name = "core",
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage
};
