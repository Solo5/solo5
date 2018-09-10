/* 
 * Copyright (c) 2015-2018 Contributors as noted in the AUTHORS file
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
 * vt.h: VT tender internal API definitions.
 *
 * Architecture and backend-independent users should not need to include any
 * other header files.
 */

#ifndef VT_H
#define VT_H

#include <inttypes.h>
#include <err.h>

#include "cc.h"
#define VT_HOST
#include "vt_guest.h"
#include "vt_gdb.h"

/*
 * Hypervisor {arch,backend}-independent data is defined here.
 * {arch,backend}-dependent data (b) is defined in vt_hv_{backend}.h.
 */
struct vt_hv {
    uint8_t *mem;
    size_t mem_size;
    struct vt_hvb *b;
};

/*
 * Load an ELF binary from (file) into (mem_size) bytes of (mem), returning
 * the entry point (gpa_ep) and last byte used by the binary (gpa_kend).
 */
void vt_elf_load(const char *file, uint8_t *mem, size_t mem_size,
        vt_gpa_t *p_entry, vt_gpa_t *p_end);                

/*
 * Check that (gpa) and (gpa + sz) are within guest memory. Returns a host-side
 * pointer to (gpa) if successful, aborts if not.
 */
#define VT_CHECKED_GPA_P(hv, gpa, sz) \
    vt_checked_gpa_p((hv), (gpa), (sz), __FILE__, __LINE__)

inline void *vt_checked_gpa_p(struct vt_hv *hv, vt_gpa_t gpa, size_t sz,
        const char *file, int line)
{    
    vt_gpa_t r;

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
struct vt_hv *vt_hv_init(size_t mem_size);

/*
 * Computes the memory size to use for this tender, based on the user-provided
 * value (rounding down if necessary).
 */
void vt_hv_mem_size(size_t *mem_size);

/*
 * Initialise VCPU state with (gpa_ep) as the entry point and (gpa_kend) as the
 * last byte of memory used by the guest binary. In (*cmdline), returns a
 * buffer with VT_CMDLINE_SIZE bytes of space for the guest command line.
 */
void vt_hv_vcpu_init(struct vt_hv *hv, vt_gpa_t gpa_ep,
        vt_gpa_t gpa_kend, char **cmdline);

/*
 * Run the VCPU. Returns on normal guest exit. Returns the exit status passed
 * from the unikernel on the final exit.
 */
int vt_hv_vcpu_loop(struct vt_hv *hv);

/*
 * Register the file descriptor (fd) for use with VT_HYPERCALL_POLL.
 */
int vt_core_register_pollfd(int fd);

/*
 * Register (fn) as the handler for hypercall (nr).
 */
typedef void (*vt_hypercall_fn_t)(struct vt_hv *hv, vt_gpa_t gpa);
int vt_core_register_hypercall(int nr, vt_hypercall_fn_t fn);

/*
 * Register (fn) as a hook for VT_HYPERCALL_HALT.
 */
typedef void (*vt_halt_fn_t)(struct vt_hv *hv, int status, void *cookie);
int vt_core_register_halt_hook(vt_halt_fn_t fn);

/*
 * Dispatch array of [VT_HYPERCALL_MAX] hypercalls. NULL = no handler.
 */
extern vt_hypercall_fn_t vt_core_hypercalls[];
int vt_core_hypercall_halt(struct vt_hv *hv, vt_gpa_t gpa);

/*
 * Register a custom vmexit handler (fn). (fn) must return 0 if the vmexit was
 * handled, -1 if not.
 */
typedef int (*vt_vmexit_fn_t)(struct vt_hv *hv);
int vt_core_register_vmexit(vt_vmexit_fn_t fn);

/*
 * Dispatch array of module-registered vmexit handlers. NULL terminated.
 */
extern vt_vmexit_fn_t vt_core_vmexits[];

/*
 * Module definition. (name) and (setup) are required, all other functions are
 * optional.
 */
struct vt_module {
    const char *name;
    int (*setup)(struct vt_hv *hv);
    int (*handle_cmdarg)(char *cmdarg);
    char *(*usage)(void);
};

/*
 * Array of compiled-in modules. NULL terminated.
 */
extern struct vt_module *vt_core_modules[];

extern struct vt_module vt_module_core;
extern struct vt_module vt_module_blk;
extern struct vt_module vt_module_net;
extern struct vt_module vt_module_gdb;
extern struct vt_module vt_module_dumpcore;

/*
 * GDB specific functions to be implemented on all backends for all
 * architectures.
 */

/*
 * Check if backend implements GDB support. Returns 0 if supported, -1 if not.
 * Note backends not implementing GDB support still need to implement the
 * remaining functions in this section, all of which should return -1.
 */
int vt_gdb_supported(void);

/*
 * Fills *reg with a stream of hexadecimal digits for each guest register
 * in GDB register order, where each register is in target endian order.
 * Returns 0 if success, -1 otherwise.
 */
int vt_gdb_read_registers(struct vt_hv *hv, uint8_t *reg, size_t *len);

/*
 * Writes all guest registers from a stream of hexadecimal digits for each
 * register in *reg. Each register in *reg is in GDB register order, and in
 * target endian order.
 * Returns 0 if success, -1 otherwise.
 */
int vt_gdb_write_registers(struct vt_hv *hv, uint8_t *reg, size_t len);

/*
 * Enable single stepping. Returns 0 if success, -1 otherwise.
 */
int vt_gdb_enable_ss(struct vt_hv *hv);

/*
 * Disable single stepping. Returns 0 if success, -1 otherwise.
 */
int vt_gdb_disable_ss(struct vt_hv *hv);

/*
 * Reads the current KVM exit code and maps it to a GDB signal value.
 * Returns 0 if success, -1 otherwise.
 */
int vt_gdb_read_last_signal(struct vt_hv *hv, int *signal);

/*
 * Add a breakpoint of type software or hardware, at address addr.  len is
 * typically the size of the breakpoint in bytes that should be inserted
 * Returns 0 if success, -1 otherwise.
 */
int vt_gdb_add_breakpoint(struct vt_hv *hv, gdb_breakpoint_type type,
                            vt_gpa_t addr, size_t len);

/*
 * Remove a breakpoint of type software or hardware, at address addr.  len is
 * typically the size of the breakpoint in bytes that should be inserted.
 * Returns 0 if success, -1 otherwise.
 */
int vt_gdb_remove_breakpoint(struct vt_hv *hv, gdb_breakpoint_type type,
                               vt_gpa_t addr, size_t len);

#endif /* VT_H */
