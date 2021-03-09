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
#include "version.h"

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

extern const struct mft1_note __solo5_mft1_note;
/*
 * Will be initialised at start-up, and used by bindings to access (and
 * modify!) the in-built manifest.
 */
struct mft *virtio_manifest = NULL;

static void _start2(void *arg __attribute__((unused)))
{
    static struct solo5_start_info si;

    si.cmdline = cmdline_parse(platform_cmdline());

    log(INFO, "            |      ___|\n");
    log(INFO, "  __|  _ \\  |  _ \\ __ \\\n");
    log(INFO, "\\__ \\ (   | | (   |  ) |\n");
    log(INFO, "____/\\___/ _|\\___/____/\n");
    log(INFO, "Solo5: Bindings version %s\n", SOLO5_VERSION);

    /*
     * Get the built-in manifest out of the ELF NOTE and validate it.
     * Once validated, it is available for access globally by the bindings.
     */
    struct mft *mft;
    size_t mft_size;
    mft_get_builtin_mft1_unconst(&__solo5_mft1_note, &mft, &mft_size);
    if (mft_validate(mft, mft_size) != 0) {
	log(ERROR, "Solo5: Built-in manifest validation failed. Aborting.\n");
	solo5_abort();
    }
    virtio_manifest = mft;

    mem_init();
    time_init();
    pci_enumerate();
    cpu_intr_enable();

    mem_lock_heap(&si.heap_start, &si.heap_size);
    solo5_exit(solo5_app_main(&si));
}

/*
 * Place the .interp section in this module, as it comes first in the link
 * order.
 */
DECLARE_ELF_INTERP

/*
 * The "ABI1" Solo5 ELF note is declared in this module.
 *
 * Virtio does not have an ABI contract (in the Solo5 sense) or use a tender,
 * so the ABI version here is always 1.
 */
ABI1_NOTE_DECLARE_BEGIN
{
    .abi_target = VIRTIO_ABI_TARGET,
    .abi_version = 1
}
ABI1_NOTE_DECLARE_END

/*
 * Pretend that we are an OpenBSD executable. See elf_abi.h for details.
 */
DECLARE_OPENBSD_NOTE
