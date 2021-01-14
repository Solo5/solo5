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

#include "solo5.h"
#include "../../bindings/lib.c"

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

static inline uint32_t get_reg32(const uint8_t* addr, int reg) {
    __asm__ volatile ("" : : : "memory");
    return *((volatile uint32_t*) (addr + reg));
}

static inline void set_reg32(uint8_t* addr, int reg, uint32_t value) {
    __asm__ volatile ("" : : : "memory");
    *((volatile uint32_t*) (addr + reg)) = value;
}

#define LEDCTL 0x00200

/* LEDCTL Bit Masks */
#define IXGBE_LED_IVRT_BASE 0x00000040
#define IXGBE_LED_BLINK_BASE 0x00000080
#define IXGBE_LED_MODE_MASK_BASE 0x0000000F
#define IXGBE_LED_OFFSET(_base, _i) ((_base) << (8 * (_i)))
#define IXGBE_LED_MODE_SHIFT(_i) (8*(_i))
#define IXGBE_LED_IVRT(_i) IXGBE_LED_OFFSET(IXGBE_LED_IVRT_BASE, _i)
#define IXGBE_LED_BLINK(_i) IXGBE_LED_OFFSET(IXGBE_LED_BLINK_BASE, _i)
#define IXGBE_LED_MODE_MASK(_i) IXGBE_LED_OFFSET(IXGBE_LED_MODE_MASK_BASE, _i)

static bool pci_acquire(void)
{
    struct solo5_pci_info info;

    if (solo5_pci_acquire("pci0", &info) != SOLO5_R_OK) {
        puts("Could not acquire 'pci0' device\n");
        return false;
    }

    uint8_t *buffer = info.bar0;

    uint32_t masked = get_reg32(buffer, LEDCTL) & ~IXGBE_LED_MODE_MASK(1);

    for (int i = 0; i < 5; i++) {
        puts("led on\n");
        set_reg32(buffer, LEDCTL, masked | (0xE << IXGBE_LED_MODE_SHIFT(1)));
        solo5_yield(solo5_clock_monotonic() + (500 * 1000 * 1000), NULL);
        puts("led off\n");
        set_reg32(buffer, LEDCTL, masked | (0xF << IXGBE_LED_MODE_SHIFT(1)));
        solo5_yield(solo5_clock_monotonic() + (500 * 1000 * 1000), NULL);
    }

    size_t size;
    uint8_t *dma;
    if (solo5_dma_acquire(&dma, &size) != SOLO5_R_OK) {
        puts("Could not acquire dma memory\n");
        return false;
    }

    if (size != 4194304) {
        puts("WRONG DMA SIZE\n");
        return false;
    }

    if (((uint64_t) dma) != (1ULL << 40)) {
        puts("WRONG DMA ADDRESS\n");
        return false;
    }

    for (size_t i = 0; i < size; i++)
        dma[i] = 0;

    return true;
}

int solo5_app_main(const struct solo5_start_info *si)
{
    puts("\n**** Solo5 standalone test_pci ****\n\n");

    puts(si->cmdline);

    if (pci_acquire()) {
        puts("SUCCESS\n");
        return SOLO5_EXIT_SUCCESS;
    }
    else {
        puts("FAILURE\n");
        return SOLO5_EXIT_FAILURE;
    }
}
