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
 * spt_main.c: Main program.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "spt.h"
#include "version.h"

extern struct spt_module __start_modules;
extern struct spt_module __stop_modules;

static void setup_modules(struct spt *spt, struct mft *mft)
{
    for (struct spt_module *m = &__start_modules; m < &__stop_modules; m++) {
        assert(m->ops.setup);
        if (m->ops.setup(spt, mft)) {
            warnx("Module `%s' setup failed", m->name);
            if (m->ops.usage) {
                warnx("Please check you have correctly specified:\n    %s",
                       m->ops.usage());
            }
            exit(1);
        }
    }

    bool fail = false;
    for (unsigned i = 0; i != mft->entries; i++) {
        if (mft->e[i].type >= MFT_RESERVED_FIRST)
            continue;
        if (!mft->e[i].attached) {
            warnx("Device '%s' of type %s declared but not attached.",
                    mft->e[i].name, mft_type_to_string(mft->e[i].type));
            fail = true;
        }
    }
    if (fail)
        errx(1, "All declared devices must be attached. "
                "See --help for syntax.");
}

static int handle_cmdarg(char *cmdarg, struct mft *mft)
{
    for (struct spt_module *m = &__start_modules; m < &__stop_modules; m++) {
        if (m->ops.handle_cmdarg) {
            if (m->ops.handle_cmdarg(cmdarg, mft) == 0) {
                return 0;
            }
        }
    }
    return -1;
}

static void handle_mem(char *cmdarg, size_t *mem_size)
{
    size_t mem;
    int rc = sscanf(cmdarg, "--mem=%zd", &mem);
    mem = mem << 20;
    if (rc != 1 || mem <= 0) {
        errx(1, "Malformed argument to --mem");
    }
    *mem_size = mem;
}

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s [ CORE OPTIONS ] [ -- ] "
            "KERNEL [ ARGS ]\n", prog);
    fprintf(stderr, "KERNEL is the filename of the unikernel to run.\n");
    fprintf(stderr, "ARGS are optional arguments passed to the unikernel.\n");
    fprintf(stderr, "Core options:\n");
    fprintf(stderr, "  [ --mem=512 ] (guest memory in MB)\n");
    fprintf(stderr, "    --help (display this help)\n");
    fprintf(stderr, "Compiled-in modules: ");
    for (struct spt_module *m = &__start_modules; m < &__stop_modules; m++) {
        assert(m->name);
        fprintf(stderr, "%s ", m->name);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "Compiled-in module options:\n");
    int nm = 0;
    for (struct spt_module *m = &__start_modules; m < &__stop_modules; m++) {
        if (m->ops.usage) {
            fprintf(stderr, "    %s\n", m->ops.usage());
            nm++;
        }
    }
    if (!nm)
        fprintf(stderr, "    (none)\n");
    exit(1);
}

static void version(const char *prog)
{
    fprintf(stderr, "%s tender, version %s\n", prog, SOLO5_VERSION);
    fprintf(stderr, "ABI version %u\n", SPT_ABI_VERSION);
    exit(0);
}

int main(int argc, char **argv)
{
    size_t mem_size = 0x20000000;
    uint64_t p_entry, p_end;
    const char *prog;
    const char *elf_filename;
    int elf_fd = -1;
    int matched;

    prog = basename(*argv);
    argc--;
    argv++;

    /*
     * Scan command line arguments, looking for the first non-option argument
     * which will be the ELF file to load. Stop if a "terminal" option such as
     * --help is encountered.
     */
    int argc1 = argc;
    char **argv1 = argv;
    while (*argv1 && *argv1[0] == '-') {
        if (strcmp("--", *argv1) == 0)
        {
            /* Consume and stop option processing */
            argc1--;
            argv1++;
            break;
        }

        if (strcmp("--help", *argv1) == 0)
            usage(prog);
        else if(strcmp("--version", *argv1) == 0)
            version(prog);

        argc1--;
        argv1++;
    }
    if (*argv1 == NULL) {
        warnx("Missing KERNEL operand");
        usage(prog);
    }
    elf_filename = *argv1;

    /*
     * Now that we have the ELF file name, verify that is binary is
     * ABI-compatible and load the MFT1 NOTE from it, as subsequent parsing of
     * the command line in the 2nd pass depends on the application-supplied
     * manifest.
     */
    elf_fd = open(elf_filename, O_RDONLY);
    if (elf_fd == -1)
        err(1, "%s: Could not open", elf_filename);

    struct abi1_info *abi1;
    size_t abi1_size;
    if (elf_load_note(elf_fd, elf_filename, ABI1_NOTE_TYPE, ABI1_NOTE_ALIGN,
                ABI1_NOTE_MAX_SIZE, (void **)&abi1, &abi1_size) == -1)
        errx(1, "%s: No Solo5 ABI information found in executable",
                elf_filename);
    if (abi1->abi_target != SPT_ABI_TARGET)
        errx(1, "%s: Executable is not for the solo5-spt target", elf_filename);
    if (abi1->abi_version != SPT_ABI_VERSION)
        errx(1, "%s: Executable requests unsupported ABI version %u",
                elf_filename, abi1->abi_version);
    free(abi1);

    struct mft *mft;
    size_t mft_size;
    if (elf_load_note(elf_fd, elf_filename, MFT1_NOTE_TYPE, MFT1_NOTE_ALIGN,
                MFT1_NOTE_MAX_SIZE, (void **)&mft, &mft_size) == -1)
        errx(1, "%s: No Solo5 manifest found in executable", elf_filename);
    if (mft_validate(mft, mft_size) == -1) {
        free(mft);
        errx(1, "%s: Solo5 manifest is invalid", elf_filename);
    }

    /*
     * Scan command line arguments in a 2nd pass, and pass options through to
     * modules to handle.
     */
    while (*argv && *argv[0] == '-') {
        if (strcmp("--", *argv) == 0) {
            /* Consume and stop option processing */
            argc--;
            argv++;
            break;
        }

        matched = 0;
        if (strncmp("--mem=", *argv, 6) == 0) {
            handle_mem(*argv, &mem_size);
            matched = 1;
            argc--;
            argv++;
        }
        if (handle_cmdarg(*argv, mft) == 0) {
            /* Handled by module, consume and go on to next arg */
            matched = 1;
            argc--;
            argv++;
        }
        if (!matched) {
            warnx("Invalid option: `%s'", *argv);
            usage(prog);
        }
    }
    assert(elf_filename == *argv);
    argc--;
    argv++;

    /*
     * TODO, maybe: No signal handlers, since that would mean adding more to the
     * seccomp policy.
     */

    struct spt *spt = spt_init(mem_size);

    elf_load(elf_fd, elf_filename, spt->mem, spt->mem_size, SPT_GUEST_MIN_BASE,
            spt_guest_mprotect, spt, &p_entry, &p_end);
    close(elf_fd);

    setup_modules(spt, mft);

    spt_boot_info_init(spt, p_end, argc, argv, mft, mft_size);

    spt_run(spt, p_entry);
}
