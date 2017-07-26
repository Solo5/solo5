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
 * ukvm_cpu_aarch64.h: CPU constants and initialisation data common to aarch64
 * backend implementations.
 */

#ifndef UKVM_CPU_AARCH64_H
#define UKVM_CPU_AARCH64_H

#ifndef _BITUL

#ifdef __ASSEMBLY__
#define _AC(X,Y)                X
#define _AT(T,X)                X
#else
#define __AC(X,Y)               (X##Y)
#define _AC(X,Y)                __AC(X,Y)
#define _AT(T,X)                ((T)(X))
#endif

#define _BITUL(x)               (_AC(1,UL) << (x))
#define _BITULL(x)              (_AC(1,ULL) << (x))

#endif

/*
 * As the design of ukvm_do_hypercall, the guest memory has been limited
 * to the area of 0 ~ 4GB. So we can design an address space layout for
 * guest as follow:
 *
 * Guest address space layout
 * ---------------------------------------------------------------
 * | 0 ~ 0xFFFFFFFF for RAM | 0x100000000 ~ 0x13FFFFFFF for MMIO |
 * ---------------------------------------------------------------
 * 0x100000000 MMIO space start
 * 0x0FFFFFFFF End of RAM space
 * 0x100000    loaded elf file (linker script dictates location)
 *   ...       unused ram
 * 0x012000    PMD
 * 0x011000    PUD
 * 0x010000    PGD, memory start for page table
 *   ...       command line arguments
 * 0x002000    ukvm_boot_info
 * 0x001000    non-cacheable page
 * 0x000000    unused ram
 */
#define AARCH64_MMIO_BASE       _AC(0x100000000, UL)
#define AARCH64_MMIO_SZ         _AC(0x40000000, UL)
#define AARCH64_GUEST_MIN_BASE  _AC(0x100000, UL)
#define AARCH64_PAGE_TABLE      _AC(0x10000, UL)
#define AARCH64_CMDLINE_BASE    _AC(0xC000, UL)
#define AARCH64_CMDLINE_SZ      (AARCH64_PAGE_TABLE - AARCH64_CMDLINE_BASE)
#define AARCH64_BOOT_INFO       _AC(0x1000, UL)
#define AARCH64_BOOT_INFO_SZ    (AARCH64_CMDLINE_BASE - AARCH64_BOOT_INFO)

#endif /* UKVM_CPU_AARCH64_H */
