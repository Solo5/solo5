#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <stdio.h>

#include "json.h"

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
    "#include \"mft_types.h\"\n"
    "\n"
    "MFT_NOTE_BEGIN\n"
    "{\n"
    "  .version = %lld, .entries = %d,\n"
    "  .e = {\n";

static const char out_resource[] = \
    "    { .name = \"%s\", .type = %s },\n";

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

    jvalue *jversion = NULL, *jresources = NULL;
    int entries = 0;

    for(jvalue **i = root->u.v; *i; ++i) {
        if (strcmp((*i)->n, "version") == 0) {
            jexpect(jint, *i, ".version");
            jversion = *i;
        }
        else if (strcmp((*i)->n, "resources") == 0) {
            jexpect(jarray, *i, ".resources");
            for (jvalue **j = (*i)->u.v; *j; ++j) {
                jexpect(jobject, *j, ".resources[]");
                entries++;
            }
            jresources = *i;
        }
        else
            errx(1, "(root): unknown key: %s", (*i)->n);
    }

    if (jversion == NULL)
        errx(1, "missing .version");
    if (jresources == NULL)
        errx(1, "missing .resources");

    printf(out_header, entries, jversion->u.i, entries);
    for (jvalue **i = jresources->u.v; *i; ++i) {
        jexpect(jobject, *i, ".resources[]");
        char *r_name = NULL, *r_type = NULL;
        for (jvalue **j = (*i)->u.v; *j; ++j) {
            if (strcmp((*j)->n, "name") == 0) {
                jexpect(jstring, *j, ".resources[...]");
                r_name = (*j)->u.s;
            }
            else if (strcmp((*j)->n, "type") == 0) {
                jexpect(jstring, *j, ".resources[...]");
                r_type = (*j)->u.s;
            }
            else
                errx(1, ".resources[...]: unknown key: %s", (*j)->n);
        }
        if (r_name == NULL)
            errx(1, ".resources[...]: missing .name");
        if (r_type == NULL)
            errx(1, ".resources[...]: missing .type");
        printf(out_resource, r_name, r_type);
    }
    printf(out_footer);

    jdel(root);
    return 0;
}
