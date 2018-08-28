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
 * ukvm_guest.h: UKVM guest-side API definitions.
 *
 * This header file must be kept self-contained with no external dependencies
 * other than C99 headers.
 *
 * This header file is dual-use; monitor code will define UKVM_HOST when
 * including it.
 */

#ifndef UKVM_GUEST_H
#define UKVM_GUEST_H

#include <stddef.h>
#include <stdint.h>

/*
 * Arch-dependent part of struct ukvm_boot_info.
 */
struct ukvm_cpu_boot_info {
    uint64_t tsc_freq;                  /* TSC frequency in Hz */
};

#ifdef __x86_64__
/*
 * PIO base address used to dispatch hypercalls.
 */
#define UKVM_HYPERCALL_PIO_BASE 0x500

#    ifdef UKVM_HOST
/*
 * Non-dereferencable monitor-side type representing a guest physical address.
 */
typedef uint64_t ukvm_gpa_t;
#    else
/*
 * On x86, 32-bit PIO is used as the hypercall mechanism. This only supports
 * sending 32-bit pointers; raise an assertion if a bigger pointer is used.
 *
 * On x86 the compiler-only memory barrier ("memory" clobber) is sufficient
 * across the hypercall boundary.
 */
static inline void ukvm_do_hypercall(int n, volatile void *arg)
{
#    ifdef assert
    assert(((uint64_t)arg <= UINT32_MAX));
#    endif
    __asm__ __volatile__("outl %0, %1"
            :
            : "a" ((uint32_t)((uint64_t)arg)),
              "d" ((uint16_t)(UKVM_HYPERCALL_PIO_BASE + n))
            : "memory");
}
#    endif

#elif defined(__aarch64__)
/*
 * MMIO base address used to dispatch hypercalls.
 * Currently, we have limited the max guest memory size to 4GB, to guarantee
 * the 32-bit pointer used for hypercall is enough.
 *
 * MMIO start from 4GB, this value can be changed by AARCH64_MMIO_BASE.
 */
#define UKVM_HYPERCALL_MMIO_BASE    (0x100000000UL)

/*
 * On aarch64, the MMIO address must be 64-bit aligned, because we configured
 * the memory attributes of MMIO space to MT_DEVICE_nGnRnE, which is not allow
 * an unaligned access. We must access this area in 64-bit.
 *
 * So the real hypercall ID will be calculated as:
 *          UKVM_HYPERCALL_MMIO_BASE + (ID << 3).
 */
#define UKVM_HYPERCALL_ADDRESS(x)   (UKVM_HYPERCALL_MMIO_BASE + ((x) << 3))
#define UKVM_HYPERCALL_NR(x)        (((x) - UKVM_HYPERCALL_MMIO_BASE) >> 3)

#    ifdef UKVM_HOST
/*
 * Non-dereferencable monitor-side type representing a guest physical address.
 */
typedef uint64_t ukvm_gpa_t;
#    else
/*
 * In order to keep consistency with x86_64, we limit this hypercall only
 * to support sending 32-bit pointers; raise an assertion if a bigger
 * pointer is used.
 *
 * On aarch64 the compiler-only memory barrier ("memory" clobber) is
 * sufficient across the hypercall boundary.
 */
static inline void ukvm_do_hypercall(int n, volatile void *arg)
{
#    ifdef assert
    assert(((uint64_t)arg <= UINT32_MAX));
#    endif
	__asm__ __volatile__("str %w0, [%1]"
	        :
	        : "rZ" ((uint32_t)((uint64_t)arg)),
	          "r" ((uint64_t)UKVM_HYPERCALL_ADDRESS(n))
	        : "memory");
}
#    endif
#else
#    error Unsupported architecture
#endif

/*
 * Guest-provided pointers in all structures shared between the guest and
 * monitor MUST be declared with UKVM_GUEST_PTR(type), where type is the
 * desired guest-side pointer type. ukvm_gpa_t (defined earlier) will be used
 * as the monitor-side type.
 *
 * This ensures that these pointers are not directly dereferencable on the
 * monitor side.
 */
#ifdef UKVM_HOST
#    define UKVM_GUEST_PTR(T) ukvm_gpa_t
#else
#    define UKVM_GUEST_PTR(T) T
#endif

/*
 * A pointer to this structure is passed by the monitor as the sole argument to
 * the guest entrypoint.
 */
struct ukvm_boot_info {
    uint64_t mem_size;                  /* Memory size in bytes */
    uint64_t kernel_end;                /* Address of end of kernel */
    UKVM_GUEST_PTR(char *) cmdline;     /* Address of command line (C string) */
    struct ukvm_cpu_boot_info cpu;      /* Arch-dependent part (see above) */
};
/*
 * Maximum size of guest command line, including the string terminator.
 */
#define UKVM_CMDLINE_SIZE 8192

/*
 * Canonical list of hypercalls supported by all monitor modules. Actual calls
 * supported at run time depend on module configuration at build time.
 */
enum ukvm_hypercall {
    /* UKVM_HYPERCALL_RESERVED=0 */
    UKVM_HYPERCALL_WALLTIME=1,
    UKVM_HYPERCALL_PUTS,
    UKVM_HYPERCALL_POLL,
    UKVM_HYPERCALL_BLKINFO,
    UKVM_HYPERCALL_BLKWRITE,
    UKVM_HYPERCALL_BLKREAD,
    UKVM_HYPERCALL_NETINFO,
    UKVM_HYPERCALL_NETWRITE,
    UKVM_HYPERCALL_NETREAD,
    UKVM_HYPERCALL_HALT,
    UKVM_HYPERCALL_MAX
};

/*
 * Hypercall definitions follow.
 */

/* UKVM_HYPERCALL_WALLTIME */
struct ukvm_walltime {
    /* OUT */
    uint64_t nsecs;
};

/* UKVM_HYPERCALL_PUTS */
struct ukvm_puts {
    /* IN */
    UKVM_GUEST_PTR(const char *) data;
    size_t len;
};

/* UKVM_HYPERCALL_BLKINFO */
struct ukvm_blkinfo {
    /* OUT */
    size_t sector_size;
    size_t num_sectors;
    int rw;
};

/* UKVM_HYPERCALL_BLKWRITE */
struct ukvm_blkwrite {
    /* IN */
    size_t sector;
    UKVM_GUEST_PTR(const void *) data;
    size_t len;

    /* OUT */
    int ret;
};

/* UKVM_HYPERCALL_BLKREAD */
struct ukvm_blkread {
    /* IN */
    size_t sector;
    UKVM_GUEST_PTR(void *) data;

    /* IN/OUT */
    size_t len;

    /* OUT */
    int ret;
};

/* UKVM_HYPERCALL_NETINFO */
struct ukvm_netinfo {
    /* OUT */
    uint8_t mac_address[6];
};

/* UKVM_HYPERCALL_NETWRITE */
struct ukvm_netwrite {
    /* IN */
    UKVM_GUEST_PTR(const void *) data;
    size_t len;

    /* OUT */
    int ret;
};

/* UKVM_HYPERCALL_NETREAD */
struct ukvm_netread {
    /* IN */
    UKVM_GUEST_PTR(void *) data;

    /* IN/OUT */
    size_t len;

    /* OUT */
    int ret;
};

/*
 * UKVM_HYPERCALL_POLL: Block until timeout_nsecs have passed or I/O is
 * possible, whichever is sooner. Returns 1 if I/O is possible, otherwise 0.
 *
 * TODO: Extend this interface to select which I/O events are of interest.
 */
struct ukvm_poll {
    /* IN */
    uint64_t timeout_nsecs;

    /* OUT */
    int ret;
};

/*
 * UKVM_HYPERCALL_HALT: Terminate guest execution.
 *
 * (exit_status) will be returned to the host.
 *
 * Additionally, the guest may supplied a (cookie) providing a hint to the
 * monitor about where e.g. a trap frame may be found in guest memory. The
 * memory area pointed to by (cookie) must allow for at least
 * UKVM_HALT_COOKIE_MAX bytes.
 */
#define UKVM_HALT_COOKIE_MAX 512

struct ukvm_halt {
    /* IN */
    UKVM_GUEST_PTR(void *) cookie;

    /* IN */
    int exit_status;
};

#endif /* UKVM_GUEST_H */
