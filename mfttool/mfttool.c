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
 * mfttool.c: Solo5 application manifest generator.
 *
 * This tool produces a C source file defining the binary manifest from its
 * JSON source. The produced C source file should be compiled with the Solo5
 * toolchain and linked into the unikernel binary.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>

#include "json.h"
#include "mft_abi.h"

static const char *jtypestr(enum jtypes t)
{
    switch (t) {
    case jnull:     return "NULL";
    case jtrue:     return "BOOLEAN";
    case jfalse:    return "BOOLEAN";
    case jstring:   return "STRING";
    case jarray:    return "ARRAY";
    case jobject:   return "OBJECT";
    case jint:      return "INTEGER";
    case jreal:     return "REAL";
    default:        return "UNKNOWN";
    }
}

static void jexpect(enum jtypes t, jvalue *v, const char *loc)
{
    if (v->d != t)
        errx(1, "%s: expected %s, got %s", loc, jtypestr(t), jtypestr(v->d));
}

static const char out_header[] = \
    "#define MFT_ENTRIES %d\n"
    "#include \"mft_abi.h\"\n"
    "\n"
    "MFT_NOTE_BEGIN\n"
    "{\n"
    "  .version = MFT_VERSION, .entries = %d,\n"
    "  .e = {\n";

static const char out_entry[] = \
    "    { .name = \"%s\", .type = MFT_%s },\n";

static const char out_footer[] = \
    "  }\n"
    "}\n"
    "MFT_NOTE_END\n";

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s COMMAND ...\n\n", prog);
    fprintf(stderr, "COMMAND is:\n");
    fprintf(stderr, "    gen SOURCE OUTPUT:\n");
    fprintf(stderr, "        Generate application manifest from SOURCE, "
            "writing to OUTPUT.\n");
    exit(EXIT_FAILURE);
}

static int mft_generate(const char *source, const char *output)
{
    FILE *sfp = fopen(source, "r");
    if (sfp == NULL)
        err(1, "Could not open %s", source);
    FILE *ofp = fopen(output, "w");
    if (ofp == NULL)
        err(1, "Could not open %s", output);

    jvalue *root = jparse(sfp);
    if (root == NULL)
        errx(1, "%s: JSON parse error", source);
    jupdate(root);
    fclose(sfp);
    jexpect(jobject, root, "(root)");

    jvalue *jversion = NULL, *jdevices = NULL;
    int entries = 0;

    for(jvalue **i = root->u.v; *i; ++i) {
        if (strcmp((*i)->n, "version") == 0) {
            jexpect(jint, *i, ".version");
            jversion = *i;
        }
        else if (strcmp((*i)->n, "devices") == 0) {
            jexpect(jarray, *i, ".devices");
            for (jvalue **j = (*i)->u.v; *j; ++j) {
                jexpect(jobject, *j, ".devices[]");
                entries++;
            }
            jdevices = *i;
        }
        else
            errx(1, "(root): unknown key: %s", (*i)->n);
    }

    if (jversion == NULL)
        errx(1, "missing .version");
    if (jdevices == NULL)
        errx(1, "missing .devices[]");

    if (jversion->u.i != MFT_VERSION)
        errx(1, ".version: invalid version %lld, expected %d", jversion->u.i,
                MFT_VERSION);
    if (entries > MFT_MAX_ENTRIES)
        errx(1, ".devices[]: too many entries, maximum %d", MFT_MAX_ENTRIES);

    fprintf(ofp, out_header, entries, entries);
    for (jvalue **i = jdevices->u.v; *i; ++i) {
        jexpect(jobject, *i, ".devices[]");
        char *r_name = NULL, *r_type = NULL;
        for (jvalue **j = (*i)->u.v; *j; ++j) {
            if (strcmp((*j)->n, "name") == 0) {
                jexpect(jstring, *j, ".devices[...]");
                r_name = (*j)->u.s;
            }
            else if (strcmp((*j)->n, "type") == 0) {
                jexpect(jstring, *j, ".devices[...]");
                r_type = (*j)->u.s;
            }
            else
                errx(1, ".devices[...]: unknown key: %s", (*j)->n);
        }
        if (r_name == NULL)
            errx(1, ".devices[...]: missing .name");
        if (r_name[0] == 0)
            errx(1, ".devices[...]: .name may not be empty");
        if (strlen(r_name) > MFT_NAME_MAX)
            errx(1, ".devices[...]: name too long");
        for (char *p = r_name; *p; p++)
            if (!isalnum((unsigned char)*p))
                errx(1, ".devices[...]: name is not alphanumeric");
        if (r_type == NULL)
            errx(1, ".devices[...]: missing .type");
        fprintf(ofp, out_entry, r_name, r_type);
    }
    fprintf(ofp, out_footer);

    fclose(ofp);
    jdel(root);
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
    const char *prog;

    prog = basename(argv[0]);

    if (argc < 2)
        usage(prog);
    if (strcmp(argv[1], "gen") == 0) {
        if (argc != 4)
            usage(prog);
        return mft_generate(argv[2], argv[3]);
    }
    else
        usage(prog);
}
