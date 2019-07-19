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

#include "bindings.h"
#include "../crt_init.h"

extern void _newstack(uint64_t stack_start, void (*tramp)(void *), void *arg);
static void _start2(void *arg) __attribute__((noreturn));

void _start(void *arg)
{
    crt_init_ssp();
    crt_init_tls();

    volatile int gdb = 1;

    serial_init();

    if (!gdb)
        log(INFO, "Solo5: Waiting for gdb...\n");
    while (gdb == 0)
        ;

    cpu_init();
    platform_init(arg);

    /*
     * Switch away from the bootstrap stack (in boot.S) as early as possible.
     */
    _newstack(platform_mem_size(), _start2, 0);
}

extern struct mft_note __solo5_manifest_note;

/* Will abort the program if any of the devices is not acquired. */
static void mft_check_all_acquired(struct mft *mft)
{
    bool fail = false;
    for (unsigned i = 0; i != mft->entries; i++) {
        if (!mft->e[i].attached) {
            log(WARN, "Solo5: Device '%s' of type %s not attached.",
                    mft->e[i].name, mft_type_to_string(mft->e[i].type));
            fail = true;
        }
    }
    if (fail) {
        log(ERROR, "Solo5: All declared devices must be attached. Aborting.\n");
        solo5_abort();
    }
}

static void _start2(void *arg __attribute__((unused)))
{
    static struct solo5_start_info si;

    si.cmdline = cmdline_parse(platform_cmdline());

    log(INFO, "            |      ___|\n");
    log(INFO, "  __|  _ \\  |  _ \\ __ \\\n");
    log(INFO, "\\__ \\ (   | | (   |  ) |\n");
    log(INFO, "____/\\___/ _|\\___/____/\n");

    struct mft *mft = &__solo5_manifest_note.m;
    size_t mft_size = __solo5_manifest_note.h.descsz;
    if (mft_validate(mft, mft_size) != 0) {
	log(ERROR, "Solo5: Built-in manifest validation failed. Aborting.\n");
	solo5_abort();
    }

    mem_init();
    time_init();
    if (pci_enumerate(mft) != 0) {
	log(ERROR, "Solo5: PCI enumeration failed. Aborting.\n");
	solo5_abort();
    }
    mft_check_all_acquired(mft);
    cpu_intr_enable();

    mem_lock_heap(&si.heap_start, &si.heap_size);
    solo5_exit(solo5_app_main(&si));
}
