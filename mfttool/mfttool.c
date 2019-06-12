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
#include <err.h>
#include <stdio.h>

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

int main(int argc, char *argv[])
{
    if (argc != 2)
        errx(1, "usage: mfttool JSON");

    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL)
        err(1, "fopen");

    jvalue *root = jparse(fp);
    if (root == NULL)
        errx(1, "parse error");
    jupdate(root);
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

    printf(out_header, entries, entries);
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
        /*
         * TODO: Validate that name is [A-Z][a-z][0-9]+.
         */
        if (r_type == NULL)
            errx(1, ".devices[...]: missing .type");
        printf(out_entry, r_name, r_type);
    }
    printf(out_footer);

    jdel(root);
    return 0;
}
