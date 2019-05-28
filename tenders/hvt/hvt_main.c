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
 * hvt_main.c: Main program.
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

#include "hvt.h"

static void setup_cmdline(char *cmdline, int argc, char **argv)
{
    size_t cmdline_free = HVT_CMDLINE_SIZE;

    cmdline[0] = 0;

    for (; *argv; argc--, argv++) {
        size_t alen = snprintf(cmdline, cmdline_free, "%s%s", *argv,
                (argc > 1) ? " " : "");
        if (alen >= cmdline_free) {
            errx(1, "Guest command line too long (max=%d characters)",
                    HVT_CMDLINE_SIZE - 1);
            break;
        }
        cmdline_free -= alen;
        cmdline += alen;
    }
}

extern struct hvt_module __start_modules;
extern struct hvt_module __stop_modules;

static void setup_modules(struct hvt *hvt)
{
    for (struct hvt_module *m = &__start_modules; m < &__stop_modules; m++) {
        assert(m->ops.setup);
        if (m->ops.setup(hvt)) {
            warnx("Module `%s' setup failed", m->name);
            if (m->ops.usage) {
                warnx("Please check you have correctly specified:\n    %s",
                       m->ops.usage());
            }
            exit(1);
        }
    }
}

static int handle_cmdarg(char *cmdarg)
{
    for (struct hvt_module *m = &__start_modules; m < &__stop_modules; m++) {
        if (m->ops.handle_cmdarg) {
            if (m->ops.handle_cmdarg(cmdarg) == 0) {
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
    fprintf(stderr, "usage: %s [ CORE OPTIONS ] [ MODULE OPTIONS ] [ -- ] "
            "KERNEL [ ARGS ]\n", prog);
    fprintf(stderr, "KERNEL is the filename of the unikernel to run.\n");
    fprintf(stderr, "ARGS are optional arguments passed to the unikernel.\n");
    fprintf(stderr, "Core options:\n");
    fprintf(stderr, "  [ --mem=512 ] (guest memory in MB)\n");
    fprintf(stderr, "    --help (display this help)\n");
    fprintf(stderr, "Compiled-in modules: ");
    for (struct hvt_module *m = &__start_modules; m < &__stop_modules; m++) {
        assert(m->name);
        fprintf(stderr, "%s ", m->name);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "Compiled-in module options:\n");
    int nm = 0;
    for (struct hvt_module *m = &__start_modules; m < &__stop_modules; m++) {
        if (m->ops.usage) {
            fprintf(stderr, "    %s\n", m->ops.usage());
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
    hvt_gpa_t gpa_ep, gpa_kend;
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

    struct sigaction sa;
    memset (&sa, 0, sizeof (struct sigaction));
    sa.sa_handler = sig_handler;
    sigfillset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1)
        err(1, "Could not install signal handler");
    if (sigaction(SIGTERM, &sa, NULL) == -1)
        err(1, "Could not install signal handler");

    hvt_mem_size(&mem_size);
    struct hvt *hvt = hvt_init(mem_size);
    uint32_t tls_size;

    elf_load(elffile, hvt->mem, hvt->mem_size, &gpa_ep, &gpa_kend, &tls_size);

    char *cmdline;
    hvt_vcpu_init(hvt, gpa_ep, gpa_kend, &cmdline);
    setup_cmdline(cmdline, argc, argv);

    setup_modules(hvt);

#if HVT_DROP_PRIVILEGES
    hvt_drop_privileges();
#else
    warnx("WARNING: Tender is configured with HVT_DROP_PRIVILEGES=0. Not"
          " dropping any privileges.");
    warnx("WARNING: This is not recommended for production use.");
#endif

    return hvt_vcpu_loop(hvt);
}
