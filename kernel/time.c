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

/*
 * Minimum delta to sleep using PIT. Programming seems to have an overhead of
 * 3-4us, but play it safe here.
 */
#define PIT_MIN_DELTA	16


#define TIMER_CNTR	0x40
#define TIMER_MODE	0x43
#define TIMER_SEL0	0x00
#define TIMER_LATCH	0x00
#define TIMER_RATEGEN	0x04
#define TIMER_ONESHOT	0x08
#define TIMER_16BIT	0x30
#define TIMER_HZ	1193182

#define	RTC_COMMAND	0x70
#define	RTC_DATA	0x71
#define RTC_NMI_DISABLE	(1<<8)
#define RTC_NMI_ENABLE	0
#define	RTC_SEC		0x00
#define	RTC_MIN		0x02
#define	RTC_HOUR	0x04
#define	RTC_DAY		0x07
#define	RTC_MONTH	0x08
#define	RTC_YEAR	0x09
#define	RTC_STATUS_A	0x0a
#define	RTC_UIP		(1<<7)

/* RTC wall time offset at monotonic time base. */
uint64_t rtc_epochoffset;

/*
 * TSC clock specific.
 */

/* Base time values at the last call to tscclock_monotonic(). */
static uint64_t time_base;
static uint64_t tsc_base;

/* Multiplier for converting TSC ticks to nsecs. (0.32) fixed point. */
static uint32_t tsc_mult;

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
volatile struct pvclock_vcpu_time_info pvclock_ti;
volatile struct pvclock_wall_clock pvclock_wc;

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

static inline uint64_t mul64_32(uint64_t a, uint32_t b) {
    uint64_t prod;
#if defined(__x86_64__)
    /* For x86_64 the computation can be done using 64-bit multiply and
     * shift. */
    __asm__ (
        "mul %%rdx ; "
        "shrd $32, %%rdx, %%rax"
        : "=a" (prod)
        : "0" (a), "d" ((uint64_t)b)
    );
#elif defined(__i386__)
    /* For i386 we compute the partial products and add them up, discarding
     * the lower 32 bits of the product in the process. */
    uint32_t h = (uint32_t)(a >> 32);
    uint32_t l = (uint32_t)a;
    uint32_t t1, t2;
    __asm__ (
        "mul  %5       ; "  /* %edx:%eax = (l * b)                    */
        "mov  %4,%%eax ; "  /* %eax = h                               */
        "mov  %%edx,%4 ; "  /* t1 = ((l * b) >> 32)                   */
        "mul  %5       ; "  /* %edx:%eax = (h * b)                    */
        "xor  %5,%5    ; "  /* t2 = 0                                 */
        "add  %4,%%eax ; "  /* %eax = (h * b) + t1 (LSW)              */
        "adc  %5,%%edx ; "  /* %edx = (h * b) + t1 (MSW)              */
        : "=A" (prod), "=r" (t1), "=r" (t2)
        : "a" (l), "1" (h), "2" (b)
    );
#else
#error mul64_32 not supported for target architecture
#endif

    return prod;
}


uint64_t pvclock_monotonic(void) {
    uint32_t version;
    uint64_t delta, time_now;

    do {
        version = pvclock_ti.version;
        __asm__ ("mfence" ::: "memory");
        delta = rdtsc() - pvclock_ti.tsc_timestamp;
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

    printf("Initializing the KVM Paravirtualized clock.\n");

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
    rtc_epochoffset = pvclock_read_wall_clock();

    return 0;
}

/*
 * Multiplier for converting nsecs to PIT ticks. (1.32) fixed point.
 *
 * Calculated as:
 *
 *     f = NSEC_PER_SEC / TIMER_HZ   (0.31) fixed point.
 *     pit_mult = 1 / f              (1.32) fixed point.
 */
static const uint32_t pit_mult
    = (1ULL << 63) / ((NSEC_PER_SEC << 31) / TIMER_HZ);


/*
 * Read the current i8254 channel 0 tick count.
 */
static unsigned int i8254_gettick(void) {
    uint16_t rdval;

    outb(TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);
    rdval = inb(TIMER_CNTR);
    rdval |= (inb(TIMER_CNTR) << 8);
    return rdval;
}

/*
 * Delay for approximately n microseconds using the i8254 channel 0 counter.
 * Timer must be programmed appropriately before calling this function.
 */
static void i8254_delay(unsigned int n) {
    unsigned int cur_tick, initial_tick;
    int remaining;
    const unsigned long timer_rval = TIMER_HZ / 100;

    initial_tick = i8254_gettick();

    remaining = (unsigned long long) n * TIMER_HZ / 1000000;

    while (remaining > 1) {
        cur_tick = i8254_gettick();
        if (cur_tick > initial_tick)
            remaining -= timer_rval - (cur_tick - initial_tick);
        else
            remaining -= initial_tick - cur_tick;
        initial_tick = cur_tick;
    }
}

/*
 * Read a RTC register. Due to PC platform braindead-ness also disables NMI.
 */
static inline uint8_t rtc_read(uint8_t reg) {

    outb(RTC_COMMAND, reg | RTC_NMI_DISABLE);
    return inb(RTC_DATA);
}

/*
 * Return current RTC time. Note that due to waiting for the update cycle to
 * complete, this call may take some time.
 */
static uint64_t rtc_gettimeofday(void) {
    struct bmk_clock_ymdhms dt;

    interrupts_disable();

    /*
     * If RTC_UIP is down, we have at least 244us to obtain a
     * consistent reading before an update can occur.
     */
    while (rtc_read(RTC_STATUS_A) & RTC_UIP)
        continue;

    dt.dt_sec = bcdtobin(rtc_read(RTC_SEC));
    dt.dt_min = bcdtobin(rtc_read(RTC_MIN));
    dt.dt_hour = bcdtobin(rtc_read(RTC_HOUR));
    dt.dt_day = bcdtobin(rtc_read(RTC_DAY));
    dt.dt_mon = bcdtobin(rtc_read(RTC_MONTH));
    dt.dt_year = bcdtobin(rtc_read(RTC_YEAR)) + 2000;

    interrupts_enable();

    return clock_ymdhms_to_secs(&dt) * NSEC_PER_SEC;
}

/*
 * Initialise i8254 timer channel 0 to mode 4 (one shot).
 */
void i8254_init() {
    outb(TIMER_MODE, TIMER_SEL0 | TIMER_ONESHOT | TIMER_16BIT);
}

/*
 * Beturn monotonic time using TSC clock.
 */
uint64_t tscclock_monotonic(void) {
    uint64_t tsc_now, tsc_delta;

    /*
     * Update time_base (monotonic time) and tsc_base (TSC time).
     */
    tsc_now = rdtsc();
    tsc_delta = tsc_now - tsc_base;
    time_base += mul64_32(tsc_delta, tsc_mult);
    tsc_base = tsc_now;

    return time_base;
}

/*
 * Calibrate TSC and initialise TSC clock.
 */
int tscclock_init(void) {
    uint64_t tsc_freq;

    /* Initialise i8254 timer channel 0 to mode 2 at 100 Hz */
    outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
    outb(TIMER_CNTR, (TIMER_HZ / 100) & 0xff);
    outb(TIMER_CNTR, (TIMER_HZ / 100) >> 8);

    /*
     * Read RTC time to use as epoch offset. This must be done just
     * before tsc_base is initialised in order to get a correct
     * offset.
     */
    rtc_epochoffset = rtc_gettimeofday();

    /*
     * Calculate TSC frequency by calibrating against an 0.1s delay
     * using the i8254 timer.
     */
    tsc_base = rdtsc();
    i8254_delay(100000);
    tsc_freq = (rdtsc() - tsc_base) * 10;
    printf("TSC frequency estimate is %lu Hz\n",
        (unsigned long long)tsc_freq);

    /*
     * Calculate TSC scaling multiplier.
     *
     * (0.32) tsc_mult = NSEC_PER_SEC (32.32) / tsc_freq (32.0)
     */
    tsc_mult = (NSEC_PER_SEC << 32) / tsc_freq;

    /*
     * Monotonic time begins at tsc_base (first read of TSC before
     * calibration).
     */
    time_base = mul64_32(tsc_base, tsc_mult);

    return 0;
}

/*
 * Return epoch offset (wall time offset to monotonic clock start).
 */
uint64_t cpu_clock_epochoffset(void) {
	return rtc_epochoffset;
}


/*
 * Returns early if any interrupts are serviced, or if the requested delay is
 * too short.
 */
void cpu_block(uint64_t until) {
    uint64_t now, delta_ns;
    uint64_t delta_ticks;
    unsigned int ticks;

    /*
     * Return if called too late.  Doing do ensures that the time
     * delta is positive.
     */
    now = solo5_clock_monotonic();
    if (until <= now)
        return;

    /*
     * Compute delta in PIT ticks. Return if it is less than minimum safe
     * amount of ticks.  Essentially this will cause us to spin until
     * the timeout.
     */
    delta_ns = until - now;
    delta_ticks = mul64_32(delta_ns, pit_mult);
    if (delta_ticks < PIT_MIN_DELTA) {
        return;
    }

    /*
     * Program the timer to interrupt the CPU after the delay has expired.
     * Maximum timer delay is 65535 ticks.
     */
    if (delta_ticks > 65535)
        ticks = 65535;
    else
        ticks = delta_ticks;

    /*
     * Note that according to the Intel 82C54 datasheet, p12 the
     * interrupt is actually delivered in N + 1 ticks.
     */
    outb(TIMER_CNTR, (ticks - 1) & 0xff);
    outb(TIMER_CNTR, (ticks - 1) >> 8);

    /*
     * Wait for any interrupt. If we got an interrupt then
     * just return into the scheduler which will check if there is
     * work to do and send us back here if not.
     *
     * TODO: It would be more efficient for longer sleeps to be
     * able to distinguish if the interrupt was the PIT interrupt
     * and no other, but this will do for now.
     */
    __asm__ __volatile__("hlt;\n");
}
