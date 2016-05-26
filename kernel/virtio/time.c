/* Copyright (c) 2015, IBM 
 * Author(s): Dan Williams <djwillia@us.ibm.com> 
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

#include "kernel.h"


/*
 * Unit definitions:
 *
 * Ticks -  This is defined as a "tick" of the oscillator in the
 *          Intel 8253 Programmable Interval Timer (PIT).                            
 *          The oscillator is fixed by the hardware to run at about
 *          1193182 ticks per second  (or, 1.193182 MHZ)
 *
 * Counts - Unit where every "count" an IRQ0 will be fired.  It is 
 *          defined such that there are 100 ticks in a count, 
 *          for ease of mathematics. The number of ticks per count 
 *          is also the number to set the reload count of the PIT to.
 *
 *
 * MS     - Milliseconds
 *
 */
 //TODO(DanB) Perhaps come up with a better name than "count" 
 //          since the "count" in "reload count" is completely 
 //          different than this unit called "count"?)

#define TICKS_PER_SEC 1193182. /* Frequency of Intel 8253 PIT 
                                  (Fixed by the hardware) */

//TODO(DanB): what ticks per count is the best number?
#define TICKS_PER_COUNT 1000  /* This is also the
                                 reload count to set PIT to */

#define COUNTS_PER_SEC (TICKS_PER_SEC / TICKS_PER_COUNT)

#define MS_PER_SEC 1000 /* milliseconds per second */
#define NS_PER_SEC 1000000000 /* ns per second */

static int use_pvclock = 0;
static volatile uint64_t counts_since_startup = 0;

/* return ns since time_init() */
uint64_t solo5_clock_monotonic(void) {
    if (use_pvclock) {
        return pvclock_monotonic();
    } else {
        float sec_since_startup = counts_since_startup /
            COUNTS_PER_SEC;

        return (uint64_t) (sec_since_startup *
            NS_PER_SEC);
    }
}


/* sleep for given seconds */
int sleep(uint32_t secs) {
    /* Step 1: convert given secs to counts */
    float counts_to_sleep = secs * COUNTS_PER_SEC;
    /* Step 2: get start time */
    uint64_t initial_count = counts_since_startup;

    //TODO(DanB): This essentially throws away the fractional part
    //            of counts_to_sleep.
    //            Figure out way to sleep more accurately
    /* wait until its time to wake up */
    while ( counts_to_sleep > 
            (counts_since_startup - initial_count) )
        ;

    return 0;
}

/* called on whenever the PIT fires (i.e. IRQ0 fires) */
void increment_time_count(void) {
    counts_since_startup++;
}

// TODO(DanB): Perhaps choose a better name?
// Name is a bit long
uint64_t time_counts_since_startup(void) {
    return counts_since_startup;
}


/* must be called before interrupts are enabled */
void time_init(void) {
    use_pvclock = !pvclock_init();

    //NOTE(DanB): inb instruction only accepts 
    //            al as operand
    /* Setup PIT */
    __asm__ __volatile__(
            "movb $0b00110100, %%al;" /* use rate generator mode */
            "outb %%al, $0x43;"
            "movb %0, %%al;" 
            "outb %%al, $0x40;" /* reload count low byte */
            "movb %1, %%al;"
            "outb %%al, $0x40;" /* reload count high byte */
             : /*no output*/: "N" (TICKS_PER_COUNT & 0xFF), 
                "N" ((TICKS_PER_COUNT >> 8) & 0xFF): "al");
        
}


/* 
 * Gets the current count in the PIT
 * used for debug purposes 
 */
uint16_t current_pit_count() {
    uint16_t count;

    //NOTE(DanB): inb instruction only accepts 
    //            al as operand
    //read timer
    __asm__ __volatile__("pushf; cli;"
            "movw $0, %%ax;"
            "outb %%al, $0x43;"
            "inb $0x40, %%al;" 
            "rol $8, %%ax;"
            "inb $0x40, %%al;" 
            "rol $8, %%ax;"
            "movw %%ax, %0;"
            "popf" : "=r" (count) :: "ax");

    return count;
}

void sleep_test(void) {
    int i;
    for(i = 0; i < 5; i++) {
        uint64_t tsc = rdtsc();
        sleep(1);
        printf("Timer freq: %u\n", rdtsc() - tsc);
    }
}
