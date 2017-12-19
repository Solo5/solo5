/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of ukvm, a unikernel monitor.
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
 * ukvm_module_exec.c: implementation of the solo5_exec hypercall.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "ukvm.h"

static void hypercall_exec(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    char *cmdline;
    struct ukvm_exec *p =
        UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_exec));
    ukvm_gpa_t gpa_ep, gpa_kend;

    /* Need to remove the protection, so we can load it with a new elf */
    if (mprotect(hv->mem, hv->mem_size,
                 PROT_READ | PROT_WRITE | PROT_EXEC) == -1)
        err(1, "GDB: Cannot remove guest memory protection");

    /*
     * XXX: This is where we (ukvm) should be checking that this new ELF is to
     * be trusted. For now, we will assume it is.
     */

    /*
     * ukvm_elf_load panics if something fails during the load, like if one of
     * the addresses in the ELF header points to memory not in the guest.
     */
    ukvm_elf_load_mem(UKVM_CHECKED_GPA_P(hv, p->elf_mem, p->elf_mem_len),
              p->elf_mem_len, hv->mem, hv->mem_size, &gpa_ep, &gpa_kend);

    ukvm_hv_vcpu_init(hv, gpa_ep, gpa_kend, (char **)&cmdline);

    /* cmdline is pointing to (hv->mem + X86_CMDLINE_BASE). */
    cmdline[0] = '\0';

    /* The guest will continue with the new memory and registers. */
}

static int setup(struct ukvm_hv *hv)
{
    assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_EXEC,
                hypercall_exec) == 0);
    return 0;
}

struct ukvm_module ukvm_module_exec = {
    .name = "exec",
    .setup = setup,
};
