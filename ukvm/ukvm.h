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

#ifndef __UKVM_H__
#define __UKVM_H__

#include <inttypes.h>
#include <err.h>

#include "ukvm_cc.h"
#define __UKVM_HOST__
#include "ukvm_guest.h"

/*
 * Hypervisor {arch,backend}-independent data is defined here.
 * {arch,backend}-dependent data (b) is defined in ukvm_hv_{backend}.h.
 */
struct ukvm_hv {
    uint8_t *mem;
    size_t mem_size;
    struct ukvm_hvb *b;
};

/*
 * Load an ELF binary from (file) into (mem_size) bytes of (mem), returning
 * the entry point (gpa_ep) and last byte used by the binary (gpa_kend).
 */
void ukvm_elf_load(const char *file, uint8_t *mem, size_t mem_size,
        ukvm_gpa_t *p_entry, ukvm_gpa_t *p_end);                

/*
 * Check that (gpa) and (gpa + sz) are within guest memory. Returns a host-side
 * pointer to (gpa) if successful, aborts if not.
 */
#define UKVM_CHECKED_GPA_P(hv, gpa, sz) \
    ukvm_checked_gpa_p((hv), (gpa), (sz), __FILE__, __LINE__)

inline void *ukvm_checked_gpa_p(struct ukvm_hv *hv, ukvm_gpa_t gpa, size_t sz,
        const char *file, int line)
{    
    ukvm_gpa_t r;

    if ((gpa >= hv->mem_size) || add_overflow(gpa, sz, r) ||
            (r >= hv->mem_size)) {
        errx(1, "%s:%d: Invalid guest access: gpa=0x%" PRIx64 ", sz=%zu",
                file, line, gpa, sz);
    }
    else {
        return (void *)(hv->mem + gpa);
    }
}

/*
 * Initialise hypervisor, with (mem_size) bytes of guest memory.
 * (hv->mem) and (hv->mem_size) are valid after this function has been called.
 */
struct ukvm_hv *ukvm_hv_init(size_t mem_size);

/*
 * Initialise VCPU state with (gpa_ep) as the entry point and (gpa_kend) as the
 * last byte of memory used by the guest binary. In (*cmdline), returns a
 * buffer with UKVM_CMDLINE_SIZE bytes of space for the guest command line.
 */
void ukvm_hv_vcpu_init(struct ukvm_hv *hv, ukvm_gpa_t gpa_ep,
        ukvm_gpa_t gpa_kend, char **cmdline);

/*
 * Run the VCPU. Returns on normal guest exit.
 */
void ukvm_hv_vcpu_loop(struct ukvm_hv *hv);

/*
 * Register the file descriptor (fd) for use with UKVM_HYPERCALL_POLL.
 */
int ukvm_core_register_pollfd(int fd);

/*
 * Register (fn) as the handler for hypercall (nr).
 */
typedef void (*ukvm_hypercall_fn_t)(struct ukvm_hv *hv, ukvm_gpa_t gpa);
int ukvm_core_register_hypercall(int nr, ukvm_hypercall_fn_t fn);

/*
 * Dispatch array of [UKVM_HYPERCALL_MAX] hypercalls. NULL = no handler.
 */
extern ukvm_hypercall_fn_t ukvm_core_hypercalls[];

/*
 * Register a custom vmexit handler (fn). (fn) must return 0 if the vmexit was
 * handled, -1 if not.
 */
typedef int (*ukvm_vmexit_fn_t)(struct ukvm_hv *hv);
int ukvm_core_register_vmexit(ukvm_vmexit_fn_t fn);

/*
 * Dispatch array of module-registered vmexit handlers. NULL terminated.
 */
extern ukvm_vmexit_fn_t ukvm_core_vmexits[];

/*
 * Module definition. (name) and (setup) are required, all other functions are
 * optional.
 */
struct ukvm_module {
    const char *name;
    int (*setup)(struct ukvm_hv *hv);
    int (*handle_cmdarg)(char *cmdarg);
    char *(*usage)(void);
};

/*
 * Array of compiled-in modules. NULL terminated.
 */
extern struct ukvm_module *ukvm_core_modules[];

extern struct ukvm_module ukvm_module_core;
extern struct ukvm_module ukvm_module_blk;
extern struct ukvm_module ukvm_module_net;
extern struct ukvm_module ukvm_module_gdb;

#endif
