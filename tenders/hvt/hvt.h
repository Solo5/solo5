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
 * hvt.h: hvt tender internal API definitions.
 *
 * Architecture and backend-independent users should not need to include any
 * other header files.
 */

#ifndef HVT_H
#define HVT_H

#include <inttypes.h>
#include <err.h>

#include "../common/cc.h"
#include "../common/elf.h"
#define HVT_HOST
#include "hvt_abi.h"
#include "hvt_gdb.h"

/*
 * HVT_DROP_PRIVILEGES (1) enables the function hvt_drop_privileges
 * please see hvt_drop_privileges for more information
 * HCT_DROP_PRIVILEGES would be disabled in a debugging scenario
 * where the hvt has to create/modify files outside it normal operating domain
 */
#ifndef HVT_DROP_PRIVILEGES
#define HVT_DROP_PRIVILEGES 1
#endif

/*
 * Hypervisor {arch,backend}-independent data is defined here.
 * {arch,backend}-dependent data (b) is defined in hvt_{backend}.h.
 */
struct hvt {
    uint8_t *mem;
    size_t mem_size;
    struct hvt_b *b;
};

/*
 * Check that (gpa) and (gpa + sz) are within guest memory. Returns a host-side
 * pointer to (gpa) if successful, aborts if not.
 */
#define HVT_CHECKED_GPA_P(hvt, gpa, sz) \
    hvt_checked_gpa_p((hvt), (gpa), (sz), __FILE__, __LINE__)

inline void *hvt_checked_gpa_p(struct hvt *hvt, hvt_gpa_t gpa, size_t sz,
        const char *file, int line)
{
    hvt_gpa_t r;

    if ((gpa >= hvt->mem_size) || add_overflow(gpa, sz, r) ||
            (r >= hvt->mem_size)) {
        errx(1, "%s:%d: Invalid guest access: gpa=0x%" PRIx64 ", sz=%zu",
                file, line, gpa, sz);
    }
    else {
        return (void *)(hvt->mem + gpa);
    }
}

/*
 * Initialise hypervisor, with (mem_size) bytes of guest memory.
 * (hvt->mem) and (hvt->mem_size) are valid after this function has been called.
 */
struct hvt *hvt_init(size_t mem_size);

/*
 * Computes the memory size to use for this tender, based on the user-provided
 * value (rounding down if necessary).
 */
void hvt_mem_size(size_t *mem_size);

/*
 * Initialise VCPU state with (gpa_ep) as the entry point and (gpa_kend) as the
 * last byte of memory used by the guest binary. In (*cmdline), returns a
 * buffer with HVT_CMDLINE_SIZE bytes of space for the guest command line.
 */
void hvt_vcpu_init(struct hvt *hvt, hvt_gpa_t gpa_ep,
        hvt_gpa_t gpa_kend, char **cmdline);

#if HVT_DROP_PRIVILEGES
/*
 * Drop privileges. This function is called by the tender before entering the
 * VCPU loop, after guest memory has been set up and all host resources have
 * been acquired. Backend implementations SHOULD drop privileges to the minimum
 * required to run the guest at this point.
 */
void hvt_drop_privileges();
#endif

/*
 * Run the VCPU. Returns on normal guest exit. Returns the exit status passed
 * from the unikernel on the final exit.
 */
int hvt_vcpu_loop(struct hvt *hvt);

/*
 * Register the file descriptor (fd) for use with HVT_HYPERCALL_POLL.
 */
int hvt_core_register_pollfd(int fd);

/*
 * Register (fn) as the handler for hypercall (nr).
 */
typedef void (*hvt_hypercall_fn_t)(struct hvt *hvt, hvt_gpa_t gpa);
int hvt_core_register_hypercall(int nr, hvt_hypercall_fn_t fn);

/*
 * Register (fn) as a hook for HVT_HYPERCALL_HALT.
 */
typedef void (*hvt_halt_fn_t)(struct hvt *hvt, int status, void *cookie);
int hvt_core_register_halt_hook(hvt_halt_fn_t fn);

/*
 * Dispatch array of [HVT_HYPERCALL_MAX] hypercalls. NULL = no handler.
 */
extern hvt_hypercall_fn_t hvt_core_hypercalls[];
int hvt_core_hypercall_halt(struct hvt *hvt, hvt_gpa_t gpa);

/*
 * Register a custom vmexit handler (fn). (fn) must return 0 if the vmexit was
 * handled, -1 if not.
 */
typedef int (*hvt_vmexit_fn_t)(struct hvt *hvt);
int hvt_core_register_vmexit(hvt_vmexit_fn_t fn);

/*
 * Dispatch array of module-registered vmexit handlers. NULL terminated.
 */
extern hvt_vmexit_fn_t hvt_core_vmexits[];

/*
 * Module definition. (name) and (setup) are required, all other functions are
 * optional.
 */
struct hvt_module_ops {
    int (*setup)(struct hvt *hvt);
    int (*handle_cmdarg)(char *cmdarg);
    char *(*usage)(void);
};

struct hvt_module {
    const char *name;
    struct hvt_module_ops ops;
};


#define BEGIN_REGISTER_MODULE(module_name) \
    static struct hvt_module __module_ ##module_name \
    __attribute((section("modules"))) \
    __attribute((used)) = { \
	.name = #module_name, \
	.ops =
	
#define END_REGISTER_MODULE };

/*
 * GDB specific functions to be implemented on all backends for all
 * architectures.
 */

/*
 * Check if backend implements GDB support. Returns 0 if supported, -1 if not.
 * Note backends not implementing GDB support still need to implement the
 * remaining functions in this section, all of which should return -1.
 */
int hvt_gdb_supported(void);

/*
 * Fills *reg with a stream of hexadecimal digits for each guest register
 * in GDB register order, where each register is in target endian order.
 * Returns 0 if success, -1 otherwise.
 */
int hvt_gdb_read_registers(struct hvt *hvt, uint8_t *reg, size_t *len);

/*
 * Writes all guest registers from a stream of hexadecimal digits for each
 * register in *reg. Each register in *reg is in GDB register order, and in
 * target endian order.
 * Returns 0 if success, -1 otherwise.
 */
int hvt_gdb_write_registers(struct hvt *hvt, uint8_t *reg, size_t len);

/*
 * Enable single stepping. Returns 0 if success, -1 otherwise.
 */
int hvt_gdb_enable_ss(struct hvt *hvt);

/*
 * Disable single stepping. Returns 0 if success, -1 otherwise.
 */
int hvt_gdb_disable_ss(struct hvt *hvt);

/*
 * Reads the current KVM exit code and maps it to a GDB signal value.
 * Returns 0 if success, -1 otherwise.
 */
int hvt_gdb_read_last_signal(struct hvt *hvt, int *signal);

/*
 * Add a breakpoint of type software or hardware, at address addr.  len is
 * typically the size of the breakpoint in bytes that should be inserted
 * Returns 0 if success, -1 otherwise.
 */
int hvt_gdb_add_breakpoint(struct hvt *hvt, gdb_breakpoint_type type,
                            hvt_gpa_t addr, size_t len);

/*
 * Remove a breakpoint of type software or hardware, at address addr.  len is
 * typically the size of the breakpoint in bytes that should be inserted.
 * Returns 0 if success, -1 otherwise.
 */
int hvt_gdb_remove_breakpoint(struct hvt *hvt, gdb_breakpoint_type type,
                               hvt_gpa_t addr, size_t len);

#endif /* HVT_H */
