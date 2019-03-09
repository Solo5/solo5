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
 * hvt_abi.h: hvt guest hypercall ABI definitions.
 *
 * This header file must be kept self-contained with no external dependencies
 * other than C99 headers.
 *
 * This header file is dual-use; tender code will define HVT_HOST when
 * including it.
 */

#ifndef HVT_GUEST_H
#define HVT_GUEST_H

#include <stddef.h>
#include <stdint.h>

/*
 * Arch-dependent part of struct hvt_boot_info.
 */
struct hvt_cpu_boot_info {
    uint64_t tsc_freq;                  /* TSC frequency in Hz */
};

#ifdef __x86_64__
/*
 * PIO base address used to dispatch hypercalls.
 */
#define HVT_HYPERCALL_PIO_BASE 0x500

#    ifdef HVT_HOST
/*
 * Non-dereferencable tender-side type representing a guest physical address.
 */
typedef uint64_t hvt_gpa_t;
#    else
/*
 * On x86, 32-bit PIO is used as the hypercall mechanism. This only supports
 * sending 32-bit pointers; raise an assertion if a bigger pointer is used.
 *
 * On x86 the compiler-only memory barrier ("memory" clobber) is sufficient
 * across the hypercall boundary.
 */
static inline void hvt_do_hypercall(int n, volatile void *arg)
{
#    ifdef assert
    assert(((uint64_t)arg <= UINT32_MAX));
#    endif
    __asm__ __volatile__("outl %0, %1"
            :
            : "a" ((uint32_t)((uint64_t)arg)),
              "d" ((uint16_t)(HVT_HYPERCALL_PIO_BASE + n))
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
#define HVT_HYPERCALL_MMIO_BASE    (0x100000000UL)

/*
 * On aarch64, the MMIO address must be 64-bit aligned, because we configured
 * the memory attributes of MMIO space to MT_DEVICE_nGnRnE, which is not allow
 * an unaligned access. We must access this area in 64-bit.
 *
 * So the real hypercall ID will be calculated as:
 *          HVT_HYPERCALL_MMIO_BASE + (ID << 3).
 */
#define HVT_HYPERCALL_ADDRESS(x)   (HVT_HYPERCALL_MMIO_BASE + ((x) << 3))
#define HVT_HYPERCALL_NR(x)        (((x) - HVT_HYPERCALL_MMIO_BASE) >> 3)

#    ifdef HVT_HOST
/*
 * Non-dereferencable tender-side type representing a guest physical address.
 */
typedef uint64_t hvt_gpa_t;
#    else
/*
 * In order to keep consistency with x86_64, we limit this hypercall only
 * to support sending 32-bit pointers; raise an assertion if a bigger
 * pointer is used.
 *
 * On aarch64 the compiler-only memory barrier ("memory" clobber) is
 * sufficient across the hypercall boundary.
 */
static inline void hvt_do_hypercall(int n, volatile void *arg)
{
#    ifdef assert
    assert(((uint64_t)arg <= UINT32_MAX));
#    endif
	__asm__ __volatile__("str %w0, [%1]"
	        :
	        : "rZ" ((uint32_t)((uint64_t)arg)),
	          "r" ((uint64_t)HVT_HYPERCALL_ADDRESS(n))
	        : "memory");
}
#    endif
#else
#    error Unsupported architecture
#endif

/*
 * Guest-provided pointers in all structures shared between the guest and
 * tender MUST be declared with HVT_GUEST_PTR(type), where type is the
 * desired guest-side pointer type. hvt_gpa_t (defined earlier) will be used
 * as the tender-side type.
 *
 * This ensures that these pointers are not directly dereferencable on the
 * tender side.
 */
#ifdef HVT_HOST
#    define HVT_GUEST_PTR(T) hvt_gpa_t
#else
#    define HVT_GUEST_PTR(T) T
#endif

/*
 * A pointer to this structure is passed by the tender as the sole argument to
 * the guest entrypoint.
 */
struct hvt_boot_info {
    uint64_t mem_size;                  /* Memory size in bytes */
    uint64_t kernel_end;                /* Address of end of kernel */
    HVT_GUEST_PTR(char *) cmdline;     /* Address of command line (C string) */
    struct hvt_cpu_boot_info cpu;      /* Arch-dependent part (see above) */
};
/*
 * Maximum size of guest command line, including the string terminator.
 */
#define HVT_CMDLINE_SIZE 8192

/*
 * Canonical list of hypercalls supported by all tender modules. Actual calls
 * supported at run time depend on module configuration at build time.
 */
enum hvt_hypercall {
    /* HVT_HYPERCALL_RESERVED=0 */
    HVT_HYPERCALL_WALLTIME=1,
    HVT_HYPERCALL_PUTS,
    HVT_HYPERCALL_POLL,
    HVT_HYPERCALL_BLKINFO,
    HVT_HYPERCALL_BLKWRITE,
    HVT_HYPERCALL_BLKREAD,
    HVT_HYPERCALL_NETINFO,
    HVT_HYPERCALL_NETWRITE,
    HVT_HYPERCALL_NETREAD,
    HVT_HYPERCALL_HALT,
    HVT_HYPERCALL_BLKDISCARD,
    HVT_HYPERCALL_MAX
};

/*
 * Hypercall definitions follow.
 */

/* HVT_HYPERCALL_WALLTIME */
struct hvt_walltime {
    /* OUT */
    uint64_t nsecs;
};

/* HVT_HYPERCALL_PUTS */
struct hvt_puts {
    /* IN */
    HVT_GUEST_PTR(const char *) data;
    size_t len;
};

/* HVT_HYPERCALL_BLKINFO */
struct hvt_blkinfo {
    /* OUT */
    size_t sector_size;
    size_t num_sectors;
    int rw;
};

/* HVT_HYPERCALL_BLKWRITE */
struct hvt_blkwrite {
    /* IN */
    size_t sector;
    HVT_GUEST_PTR(const void *) data;
    size_t len;

    /* OUT */
    int ret;
};

/* HVT_HYPERCALL_BLKREAD */
struct hvt_blkread {
    /* IN */
    size_t sector;
    HVT_GUEST_PTR(void *) data;

    /* IN/OUT */
    size_t len;

    /* OUT */
    int ret;
};

/* HVT_HYPERCALL_NETINFO */
struct hvt_netinfo {
    /* OUT */
    uint8_t mac_address[6];
};

/* HVT_HYPERCALL_NETWRITE */
struct hvt_netwrite {
    /* IN */
    HVT_GUEST_PTR(const void *) data;
    size_t len;

    /* OUT */
    int ret;
};

/* HVT_HYPERCALL_NETREAD */
struct hvt_netread {
    /* IN */
    HVT_GUEST_PTR(void *) data;

    /* IN/OUT */
    size_t len;

    /* OUT */
    int ret;
};

/*
 * HVT_HYPERCALL_POLL: Block until timeout_nsecs have passed or I/O is
 * possible, whichever is sooner. Returns 1 if I/O is possible, otherwise 0.
 *
 * TODO: Extend this interface to select which I/O events are of interest.
 */
struct hvt_poll {
    /* IN */
    uint64_t timeout_nsecs;

    /* OUT */
    int ret;
};

/*
 * HVT_HYPERCALL_HALT: Terminate guest execution.
 *
 * (exit_status) will be returned to the host.
 *
 * Additionally, the guest may supply a (cookie) providing a hint to the
 * tender about where e.g. a trap frame may be found in guest memory. The
 * memory area pointed to by (cookie) must allow for at least
 * HVT_HALT_COOKIE_MAX bytes.
 */
#define HVT_HALT_COOKIE_MAX 512

struct hvt_halt {
    /* IN */
    HVT_GUEST_PTR(void *) cookie;

    /* IN */
    int exit_status;
};

struct hvt_blkdiscard {
    /* IN */
    size_t sector;

    /* IN */
    size_t len_sectors;

    /* OUT */
    int ret;
};

#endif /* HVT_GUEST_H */
