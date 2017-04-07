/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a unikernel base layer.
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

/*-
 * Copyright (c) 2014, 2015 Antti Kantee.  All Rights Reserved.
 * Copyright (c) 2015 Martin Lucina.  All Rights Reserved.
 * Modified for solo5 by Ricardo Koller <kollerr@us.ibm.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "kernel.h"

/* Wall time offset at monotonic time base. */
static uint64_t wc_epochoffset;

/* Xen/KVM per-vcpu time ABI. */
struct pvclock_vcpu_time_info {
    uint32_t version;
    uint32_t pad0;
    uint64_t tsc_timestamp;
    uint64_t system_time;
    uint32_t tsc_to_system_mul;
    int8_t tsc_shift;
    uint8_t flags;
    uint8_t pad[2];
} __attribute__((__packed__));

/* Xen/KVM wall clock ABI. */
struct pvclock_wall_clock {
    uint32_t version;
    uint32_t sec;
    uint32_t nsec;
} __attribute__((__packed__));

/*
 * pvclock structures shared with hypervisor.
 * TODO: These should be pointers (for Xen HVM support), but we can't use
 * bmk_pgalloc() here.
 */
static volatile struct pvclock_vcpu_time_info pvclock_ti;
static volatile struct pvclock_wall_clock pvclock_wc;

static inline void
x86_cpuid(uint32_t level, uint32_t *eax_out, uint32_t *ebx_out,
        uint32_t *ecx_out, uint32_t *edx_out)
{
    uint32_t eax_, ebx_, ecx_, edx_;

    __asm__(
        "cpuid"
        : "=a" (eax_), "=b" (ebx_), "=c" (ecx_), "=d" (edx_)
        : "0" (level)
    );
    *eax_out = eax_;
    *ebx_out = ebx_;
    *ecx_out = ecx_;
    *edx_out = edx_;
}

uint64_t pvclock_monotonic(void) {
    uint32_t version;
    uint64_t delta, time_now;

    do {
        version = pvclock_ti.version;
        __asm__ ("mfence" ::: "memory");
        delta = cpu_rdtsc() - pvclock_ti.tsc_timestamp;
        if (pvclock_ti.tsc_shift < 0)
            delta >>= -pvclock_ti.tsc_shift;
        else
            delta <<= pvclock_ti.tsc_shift;
        time_now = mul64_32(delta, pvclock_ti.tsc_to_system_mul) +
            pvclock_ti.system_time;
        __asm__ ("mfence" ::: "memory");
    } while ((pvclock_ti.version & 1) || (pvclock_ti.version != version));

    return time_now;
}

/*
 * Read wall time offset since system boot using PV clock.
 */
static uint64_t pvclock_read_wall_clock(void)
{
	uint32_t version;
	uint64_t wc_boot;

	do {
		version = pvclock_wc.version;
		__asm__ ("mfence" ::: "memory");
		wc_boot = pvclock_wc.sec * NSEC_PER_SEC;
		wc_boot += pvclock_wc.nsec;
		__asm__ ("mfence" ::: "memory");
	} while ((pvclock_wc.version & 1) || (pvclock_wc.version != version));

	return wc_boot;
}

int pvclock_init(void) {
    uint32_t eax, ebx, ecx, edx;
    uint32_t msr_kvm_system_time, msr_kvm_wall_clock;

    /*
     * Prefer new-style MSRs, and bail entirely if neither is indicated as
     * available by CPUID.
     */
    x86_cpuid(0x40000001, &eax, &ebx, &ecx, &edx);
    if (eax & (1 << 3)) {
        msr_kvm_system_time = 0x4b564d01;
        msr_kvm_wall_clock = 0x4b564d00;
    }
    else if (eax & (1 << 0)) {
        msr_kvm_system_time = 0x12;
        msr_kvm_wall_clock = 0x11;
    }
    else {
        return 1;
    }

    log(INFO, "Solo5: Clock source: KVM paravirtualized clock\n");

    __asm__ __volatile("wrmsr" ::
        "c" (msr_kvm_system_time),
        "a" ((uint32_t)((uintptr_t)&pvclock_ti | 0x1)),
        "d" ((uint32_t)((uintptr_t)&pvclock_ti >> 32))
    );
    __asm__ __volatile("wrmsr" ::
        "c" (msr_kvm_wall_clock),
        "a" ((uint32_t)((uintptr_t)&pvclock_wc)),
        "d" ((uint32_t)((uintptr_t)&pvclock_wc >> 32))
    );
    /* Initialise epoch offset using wall clock time */
    wc_epochoffset = pvclock_read_wall_clock();

    return 0;
}

/*
 * Return epoch offset (wall time offset to monotonic clock start).
 */
uint64_t pvclock_epochoffset(void) {
	return wc_epochoffset;
}
