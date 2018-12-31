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
 * spt_main.c: Main program.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "spt.h"

static void setup_cmdline(char *cmdline, int argc, char **argv)
{
    size_t cmdline_free = SPT_CMDLINE_SIZE;

    cmdline[0] = 0;

    for (; *argv; argc--, argv++) {
        size_t alen = snprintf(cmdline, cmdline_free, "%s%s", *argv,
                (argc > 1) ? " " : "");
        if (alen >= cmdline_free) {
            errx(1, "Guest command line too long (max=%d characters)",
                    SPT_CMDLINE_SIZE - 1);
            break;
        }
        cmdline_free -= alen;
        cmdline += alen;
    }
}

static void setup_modules(struct spt *spt)
{
    for (struct spt_module **m = spt_core_modules; *m; m++) {
        assert((*m)->setup);
        if ((*m)->setup(spt)) {
            warnx("Module `%s' setup failed", (*m)->name);
            if ((*m)->usage) {
                warnx("Please check you have correctly specified:\n    %s",
                       (*m)->usage());
            }
            exit(1);
        }
    }
}

static int handle_cmdarg(char *cmdarg)
{
    for (struct spt_module **m = spt_core_modules; *m; m++) {
        if ((*m)->handle_cmdarg) {
            if ((*m)->handle_cmdarg(cmdarg) == 0) {
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
    for (struct spt_module **m = spt_core_modules; *m; m++) {
        assert((*m)->name);
        fprintf(stderr, "%s ", (*m)->name);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "Compiled-in module options:\n");
    int nm = 0;
    for (struct spt_module **m = spt_core_modules; *m; m++) {
        if ((*m)->usage) {
            fprintf(stderr, "    %s\n", (*m)->usage());
            nm++;
        }
    }
    if (!nm)
        fprintf(stderr, "    (none)\n");
    exit(1);
}

int main(int argc, char **argv)
{
    size_t mem_size = 0x20000000;
    uint64_t p_entry, p_end;
    const char *prog;
    const char *elffile;
    int matched;

    prog = basename(*argv);
    argc--;
    argv++;

    while (*argv && *argv[0] == '-') {
        if (strcmp("--help", *argv) == 0)
            usage(prog);

        if (strcmp("--", *argv) == 0) {
            /* Consume and stop arg processing */
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
        if (handle_cmdarg(*argv) == 0) {
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

    /* At least one non-option argument required */
    if (*argv == NULL) {
        warnx("Missing KERNEL operand");
        usage(prog);
    }
    elffile = *argv;
    argc--;
    argv++;

    /*
     * TODO, maybe: No signal handlers, since that would mean adding more to the
     * seccomp policy.
     */

    struct spt *spt = spt_init(mem_size);

    spt_elf_load(elffile, spt->mem, spt->mem_size, &p_entry, &p_end);

    char *cmdline;
    spt_bi_init(spt, p_end, &cmdline);
    setup_cmdline(cmdline, argc, argv);

    setup_modules(spt);

    spt_run(spt, p_entry);
}
