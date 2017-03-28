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

#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <libgen.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "ukvm.h"

static void setup_cmdline(char *cmdline, int argc, char **argv)
{
    size_t cmdline_free = UKVM_CMDLINE_SIZE;

    cmdline[0] = 0;

    for (; *argv; argc--, argv++) {
        size_t alen = snprintf(cmdline, cmdline_free, "%s%s", *argv,
                (argc > 1) ? " " : "");
        if (alen >= cmdline_free) {
            errx(1, "Guest command line too long (max=%d characters)",
                    UKVM_CMDLINE_SIZE - 1);
            break;
        }
        cmdline_free -= alen;
        cmdline += alen;
    }
}

static void setup_modules(struct ukvm_hv *hv)
{
    for (struct ukvm_module **m = ukvm_core_modules; *m; m++) {
        assert((*m)->setup);
        if ((*m)->setup(hv)) {
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
    for (struct ukvm_module **m = ukvm_core_modules; *m; m++) {
        if ((*m)->handle_cmdarg) {
            if ((*m)->handle_cmdarg(cmdarg) == 0) {
                return 0;
            }
        }
    }
    return -1;
}

static void sig_handler(int signo)
{
    errx(1, "Exiting on signal %d", signo);
}

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s [ CORE OPTIONS ] [ MODULE OPTIONS ] [ -- ] "
            "KERNEL [ ARGS ]\n", prog);
    fprintf(stderr, "KERNEL is the filename of the unikernel to run.\n");
    fprintf(stderr, "ARGS are optional arguments passed to the unikernel.\n");
    fprintf(stderr, "Core options:\n");
    fprintf(stderr, "    --help (display this help)\n");
    fprintf(stderr, "Compiled-in modules: ");
    for (struct ukvm_module **m = ukvm_core_modules; *m; m++) {
        assert((*m)->name);
        fprintf(stderr, "%s ", (*m)->name);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "Compiled-in module options:\n");
    int nm = 0;
    for (struct ukvm_module **m = ukvm_core_modules; *m; m++) {
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
    ukvm_gpa_t gpa_ep, gpa_kend;
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

    struct sigaction sa;
    memset (&sa, 0, sizeof (struct sigaction));
    sa.sa_handler = sig_handler;
    sigfillset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1)
        err(1, "Could not install signal handler");
    if (sigaction(SIGTERM, &sa, NULL) == -1)
        err(1, "Could not install signal handler");

    struct ukvm_hv *hv = ukvm_hv_init(mem_size);

    ukvm_elf_load(elffile, hv->mem, hv->mem_size, &gpa_ep, &gpa_kend);

    char *cmdline;
    ukvm_hv_vcpu_init(hv, gpa_ep, gpa_kend, &cmdline);
    setup_cmdline(cmdline, argc, argv);

    setup_modules(hv);

    ukvm_hv_vcpu_loop(hv);

    return 0;
}
