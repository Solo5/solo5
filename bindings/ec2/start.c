/*
 * Copyright (c) 2015-2020 Contributors as noted in the AUTHORS file
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
 * Entry point and start-up, as on virtio (whose boot.S jumps here), plus
 * the --solo5:block= option that names which EBS volume backs a block device.
 */

#include "bindings.h"
#include "../crt_init.h"
#include "version.h"

/* --solo5:block=NAME:DEVNAME attaches NAME to the EBS volume DEVNAME. */
#define EC2_MAX_BLOCK_ALIASES 16
#define EC2_ALIAS_SIZE        64
#define EC2_CMDLINE_SIZE      8192 /* as in virtio/platform.c */
#define SOLO5_OPT_PREFIX      "--solo5:"

extern void _newstack(uint64_t stack_start, void (*tramp)(void *), void *arg);
static void _start2(void *arg) __attribute__((noreturn));

void _start(void *arg)
{
    crt_init_ssp();
    crt_init_tls();

    serial_init();

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
struct mft *ec2_manifest = NULL;

static struct {
    char name[EC2_ALIAS_SIZE];
    char devname[EC2_ALIAS_SIZE];
} block_aliases[EC2_MAX_BLOCK_ALIASES];
static unsigned block_aliases_num;

const char *ec2_block_manifest_name(const char *devname)
{
    for (unsigned i = 0; i < block_aliases_num; i++) {
        if (strcmp(block_aliases[i].devname, devname) == 0)
            return block_aliases[i].name;
    }
    return devname;
}

static bool copy_word(char *dst, const char *src, size_t len)
{
    if (len == 0 || len >= EC2_ALIAS_SIZE)
        return false;
    memcpy(dst, src, len);
    dst[len] = '\0';
    return true;
}

/* Removes the --solo5:block= options from (cmdline). */
static const char *parse_block_aliases(const char *cmdline)
{
    static char rest[EC2_CMDLINE_SIZE];
    const char opt[] = "--solo5:block=";
    const char *p = cmdline;
    size_t n = 0;

    for (;;) {
        while (*p && isspace(*p))
            p++;
        if (strncmp(p, SOLO5_OPT_PREFIX, sizeof(SOLO5_OPT_PREFIX) - 1) != 0)
            break;
        const char *end = p;
        while (*end && !isspace(*end))
            end++;
        if (strncmp(p, opt, sizeof(opt) - 1) == 0) {
            const char *name = p + sizeof(opt) - 1;
            const char *colon = name;
            while (colon < end && *colon != ':')
                colon++;
            if (colon == end || block_aliases_num == EC2_MAX_BLOCK_ALIASES ||
                !copy_word(block_aliases[block_aliases_num].name, name,
                           colon - name) ||
                !copy_word(block_aliases[block_aliases_num].devname, colon + 1,
                           end - colon - 1)) {
                log(WARN, "Solo5: ignoring malformed --solo5:block option\n");
            } else {
                log(INFO, "Solo5: block device '%s' is EBS device '%s'\n",
                    block_aliases[block_aliases_num].name,
                    block_aliases[block_aliases_num].devname);
                block_aliases_num++;
            }
        } else if ((size_t)(end - p) + 1 < sizeof(rest) - n) {
            memcpy(rest + n, p, end - p);
            n += end - p;
            rest[n++] = ' ';
        }
        p = end;
    }
    size_t len = strlen(p);
    if (len >= sizeof(rest) - n)
        len = sizeof(rest) - n - 1;
    memcpy(rest + n, p, len);
    rest[n + len] = '\0';
    return rest;
}

/* Will abort the program if any of the devices is not acquired. */
static void mft_check_all_acquired(void)
{
    bool fail = false;
    for (unsigned i = 0; i != ec2_manifest->entries; i++) {
        if (i > 0 && !ec2_manifest->e[i].attached) {
            log(WARN, "Solo5: Device '%s' of type %s not attached.\n",
                ec2_manifest->e[i].name,
                mft_type_to_string(ec2_manifest->e[i].type));
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

    si.cmdline = cmdline_parse(parse_block_aliases(platform_cmdline()));

    log(INFO, "            |      ___|\n");
    log(INFO, "  __|  _ \\  |  _ \\ __ \\\n");
    log(INFO, "\\__ \\ (   | | (   |  ) |\n");
    log(INFO, "____/\\___/ _|\\___/____/\n");
    log(INFO, "Solo5: Bindings version %s (ec2)\n", SOLO5_VERSION);

    /*
     * Get the built-in manifest out of the ELF NOTE and validate it.
     * Once validated, it is available for access globally by the bindings.
     */
    const struct mft *mft;
    size_t mft_size;
    mft_get_builtin_mft1(&__solo5_mft1_note, &mft, &mft_size);
    if (mft_validate(mft, mft_size) != 0) {
        log(ERROR, "Solo5: Built-in manifest validation failed. Aborting.\n");
        solo5_abort();
    }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    ec2_manifest = (struct mft *)mft;
#pragma GCC diagnostic pop

    /*
     * Attach the first entry if its type is really MFT_RESERVED_FIRST.
     * Abort otherwise.
     */
    const struct mft_entry *e =
        mft_get_by_index(ec2_manifest, 0, MFT_RESERVED_FIRST);
    if (e == NULL) {
        log(ERROR, "Solo5: first entry in manifest is not of type "
                   "MFT_RESERVED_FIRST\n");
        solo5_abort();
    }

    mem_init();
    time_init();
    if (pci_enumerate() != 0) {
        log(ERROR, "Solo5: PCI enumeration failed. Aborting.\n");
        solo5_abort();
    }
    mft_check_all_acquired();
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
 * Like virtio, ec2 does not use a tender, so the ABI version here is always 1.
 */
ABI1_NOTE_DECLARE_BEGIN{.abi_target = EC2_ABI_TARGET,
                        .abi_version = 1} ABI1_NOTE_DECLARE_END

    /*
     * Pretend that we are an OpenBSD executable. See elf_abi.h for details.
     */
    DECLARE_OPENBSD_NOTE
