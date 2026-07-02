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
 * acpi.c: minimal ACPI support for virtio.
 *
 * Just enough of the ACPI machinery to perform an S5 ("soft off") power-off.
 * Unlike hvt or xen, virtio has no hypercall to ask the host to shut us down,
 * so the only portable way to power off (on QEMU as well as cloud hypervisors
 * such as Google Cloud) is to drive the ACPI PM1 control register ourselves.
 *
 * The procedure is:
 *   1. Find the RSDP by scanning the BIOS area (0xe0000 - 0xfffff), which is
 *      identity-mapped (see pagetable.S).
 *   2. Walk the RSDT (or XSDT) to find the FADT ("FACP").
 *   3. Read PM1a_CNT_BLK (and PM1b_CNT_BLK) from the FADT.
 *   4. Parse the DSDT pointed to by the FADT to find the \_S5_ package, whose
 *      first element is the SLP_TYPa value to write.
 *   5. Write (SLP_TYPa << 10) | SLP_EN to the PM1 control register(s).
 *
 * If anything is missing or malformed we simply return; platform_exit() will
 * then fall back to halting the CPU. For more details about the implementation,
 * the reader should take a look on these links:
 *
 *   https://forum.osdev.org/viewtopic.php?t=16990
 *   https://wiki.osdev.org/ACPI
 *
 * This code is a mix between what Unikraft proposes for ACPI and what links
 * above describe. The initial trials did not have access to the ACPI tables
 * (available below 4GB). A modification to [pagetable.S] was therefore
 * necessary in order to dereference pointers at these locations and locate the
 * \_S5_ block. A description of the memory layout can be found here (SeaBIOS):
 *
 *   https://seabios.org/Memory_Model.html
 *
 * The attempts can be viewed here:
 *
 *   https://github.com/Solo5/solo5/pull/500/
 *   https://github.com/Solo5/solo5/pull/501/
 */

#include "bindings.h"

#define BIOS_AREA_START 0xe0000
#define BIOS_AREA_END   0xfffff
#define RSDP10_LEN      20

/* PM1 control register: SLP_TYP is bits 10-12, SLP_EN is bit 13. */
#define ACPI_SLP_EN             (1 << 13)
#define ACPI_SLP_TYP_SHIFT      10
#define ACPI_OEM_ID_LEN         6
#define ACPI_OEM_TAB_ID_LEN     8
#define ACPI_RSDP_SIG           "RSD PTR "
#define ACPI_RSDP_SIG_LEN       8
#define ACPI_SDT_SIG_LEN        4
#define ACPI_SDT_CREATOR_ID_LEN 4

struct __attribute__((__packed__)) acpi_rsdp {
    char sig[ACPI_RSDP_SIG_LEN]; /* "RSD PTR " */
    uint8_t checksum;
    char oem_id[ACPI_OEM_ID_LEN];
    uint8_t revision;
    uint32_t rsdt_paddr;
    /* The following fields only exist for revision >= 2 (ACPI 2.0+). */
    uint32_t tab_len;
    uint64_t xsdt_paddr;
    uint8_t xchecksum;
    uint8_t reserved[3];
};

struct __attribute__((__packed__)) acpi_sdt_hdr {
    char sig[ACPI_SDT_SIG_LEN];
    uint32_t tab_len;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[ACPI_OEM_ID_LEN];
    char oem_table_id[ACPI_OEM_TAB_ID_LEN];
    uint32_t oem_revision;
    char creator_id[ACPI_SDT_CREATOR_ID_LEN];
    uint32_t creator_revision;
};

_Static_assert(sizeof(struct acpi_sdt_hdr) == 36,
               "acpi_sdt_hdr must match the 36-byte ACPI SDT header layout");

/*
 * The 64-bit X_DSDT field lives at offset 140 in the FADT (ACPI 2.0+). We read
 * it by offset rather than growing struct acpi_fadt with all the intermediate
 * fields. It is only present when the FADT is at least this long.
 */
#define ACPI_FADT_X_DSDT_OFF     140
#define ACPI_FADT_X_DSDT_MIN_LEN (ACPI_FADT_X_DSDT_OFF + 8)

struct __attribute__((__packed__)) acpi_fadt {
    struct acpi_sdt_hdr hdr;
    uint32_t facs_paddr;
    uint32_t dsdt_paddr;
    uint8_t reserved0;
    uint8_t pref_pm_prof;
    uint16_t sci_irq;
    uint32_t smi_cmd;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_req;
    uint8_t pstate_ctlr;
    uint32_t pm1a_evt_blk;
    uint32_t pm1b_evt_blk;
    uint32_t pm1a_ctlr_blk;
    uint32_t pm1b_ctlr_blk;
    /* ... more fields follow but we don't need them. */
};

static uint8_t acpi_checksum(const void *base, size_t len)
{
    const uint8_t *ptr_end = (const uint8_t *)base + len;
    const uint8_t *ptr = base;
    uint8_t checksum = 0;

    while (ptr < ptr_end)
        checksum += *ptr++;

    return checksum;
}

/* Scan the BIOS area for the RSDP, validating the v1 checksum. */
static const struct acpi_rsdp *acpi_find_rsdp(void)
{
    /* The RSDP is 16-byte aligned within the BIOS area. */
    for (uintptr_t addr = BIOS_AREA_START; addr < BIOS_AREA_END; addr += 16) {
        const struct acpi_rsdp *rsdp = (const struct acpi_rsdp *)addr;

        if (memcmp(rsdp->sig, ACPI_RSDP_SIG, ACPI_RSDP_SIG_LEN) != 0)
            continue;
        /* The v1 checksum covers the first 20 bytes. */
        if (acpi_checksum(rsdp, RSDP10_LEN) != 0)
            continue;

        return rsdp;
    }

    return NULL;
}

struct acpi_root {
    const uint8_t *array;
    size_t entries;
    size_t entry_size;
};

static int acpi_root_open(const struct acpi_rsdp *rsdp, struct acpi_root *r)
{
    const struct acpi_sdt_hdr *root;

    if (rsdp->revision >= 2 && rsdp->xsdt_paddr != 0) {
        root = (const struct acpi_sdt_hdr *)(uintptr_t)rsdp->xsdt_paddr;
        r->entry_size = sizeof(uint64_t);
    } else /* rsdp->revision == 0 */ {
        root = (const struct acpi_sdt_hdr *)(uintptr_t)rsdp->rsdt_paddr;
        r->entry_size = sizeof(uint32_t);
    }

    if (root->tab_len < sizeof(struct acpi_sdt_hdr))
        return -1;

    r->entries = (root->tab_len - sizeof(struct acpi_sdt_hdr)) / r->entry_size;
    r->array = (const uint8_t *)root + sizeof(struct acpi_sdt_hdr);
    return 0;
}

static const struct acpi_sdt_hdr *acpi_root_entry(const struct acpi_root *r,
                                                  size_t i)
{
    uintptr_t phys;

    if (r->entry_size == sizeof(uint64_t)) {
        uint64_t v;
        memcpy(&v, r->array + i * r->entry_size, sizeof v);
        phys = (uintptr_t)v;
    } else {
        uint32_t v;
        memcpy(&v, r->array + i * r->entry_size, sizeof v);
        phys = (uintptr_t)v;
    }

    return (const struct acpi_sdt_hdr *)phys;
}

static const struct acpi_sdt_hdr *acpi_find_table(const struct acpi_rsdp *rsdp,
                                                  const char *signature)
{
    struct acpi_root r;

    if (acpi_root_open(rsdp, &r) != 0)
        return NULL;

    for (size_t i = 0; i < r.entries; i++) {
        const struct acpi_sdt_hdr *sdt = acpi_root_entry(&r, i);
        if (memcmp(sdt->sig, signature, 4) == 0)
            return sdt;
    }

    return NULL;
}

static const struct acpi_sdt_hdr *acpi_get_dsdt(const struct acpi_fadt *fadt)
{
    uintptr_t dsdt_paddr = fadt->dsdt_paddr;

    if (dsdt_paddr == 0 && fadt->hdr.tab_len >= ACPI_FADT_X_DSDT_MIN_LEN) {
        uint64_t x_dsdt;
        memcpy(&x_dsdt, (const uint8_t *)fadt + ACPI_FADT_X_DSDT_OFF,
               sizeof x_dsdt);
        dsdt_paddr = (uintptr_t)x_dsdt;
    }

    if (dsdt_paddr == 0)
        return NULL;

    const struct acpi_sdt_hdr *dsdt = (const struct acpi_sdt_hdr *)dsdt_paddr;
    if (memcmp(dsdt->sig, "DSDT", 4) != 0)
        return NULL;

    return dsdt;
}

static int acpi_scan_table_for_s5(const struct acpi_sdt_hdr *tbl,
                                  uint8_t *slp_typa)
{
    if (tbl->tab_len < sizeof(struct acpi_sdt_hdr))
        return -1;

    const uint8_t *aml = (const uint8_t *)tbl + sizeof(struct acpi_sdt_hdr);
    size_t aml_len = tbl->tab_len - sizeof(struct acpi_sdt_hdr);

    for (size_t i = 0; i + 4 < aml_len; i++) {
        if (memcmp(aml + i, "_S5_", 4) != 0)
            continue;

        /*
         * aml[i .. i+3] is "_S5_". A valid NameOp (0x08) optionally preceded by
         * a RootChar ('\\', 0x5c) precedes it. After the name comes a
         * PackageOp.
         */
        const uint8_t *p = aml + i + 4;
        const uint8_t *end = aml + aml_len;

        if (p >= end || *p != 0x12 /* PackageOp */) {
            log(WARN, "Solo5: ACPI: _S5_ at +%u not a package (op=0x%x)\n",
                (unsigned)i, p < end ? *p : 0);
            continue;
        }
        p++; /* skip PackageOp */

        if (p >= end)
            break;
        /*
         * PkgLength: the top two bits of the first byte give the number of
         * extra length bytes that follow. Skip the whole field.
         */
        p += 1 + (*p >> 6);

        if (p >= end)
            break;
        p++; /* skip NumElements */

        if (p >= end)
            break;

        /* Decode the first package element as SLP_TYPa. */
        if (*p == 0x0a /* BytePrefix */) {
            if (p + 1 >= end)
                break;
            *slp_typa = p[1];
        } else {
            /* ZeroOp (0x00), OneOp (0x01), or a raw small value. */
            *slp_typa = *p;
        }

        return 0;
    }

    return -1;
}

static int acpi_find_s5(const struct acpi_rsdp *rsdp,
                        const struct acpi_fadt *fadt, uint8_t *slp_typa)
{
    const struct acpi_sdt_hdr *dsdt = acpi_get_dsdt(fadt);
    if (dsdt != NULL && acpi_scan_table_for_s5(dsdt, slp_typa) == 0)
        return 0;

    struct acpi_root r;
    if (acpi_root_open(rsdp, &r) != 0)
        return -1;

    for (size_t i = 0; i < r.entries; i++) {
        const struct acpi_sdt_hdr *tbl = acpi_root_entry(&r, i);
        if (memcmp(tbl->sig, "SSDT", 4) != 0)
            continue;
        if (acpi_scan_table_for_s5(tbl, slp_typa) == 0)
            return 0;
    }

    return -1;
}

void acpi_poweroff(void)
{
    const struct acpi_rsdp *rsdp = acpi_find_rsdp();
    if (rsdp == NULL) {
        log(WARN, "Solo5: ACPI: RSDP not found, cannot power off\n");
        return;
    }

    const struct acpi_fadt *fadt =
        (const struct acpi_fadt *)acpi_find_table(rsdp, "FACP");
    if (fadt == NULL) {
        log(WARN, "Solo5: ACPI: FADT not found, cannot power off\n");
        return;
    }

    uint8_t slp_typa = 0;
    if (acpi_find_s5(rsdp, fadt, &slp_typa) != 0) {
        log(WARN, "Solo5: ACPI: \\_S5_ not found in DSDT or any SSDT, "
                  "cannot power off\n");
        return;
    }

    uint16_t value = (uint16_t)((slp_typa << ACPI_SLP_TYP_SHIFT) | ACPI_SLP_EN);

    log(INFO, "Solo5: ACPI: power off via PM1a=0x%x PM1b=0x%x SLP_TYPa=%u\n",
        fadt->pm1a_ctlr_blk, fadt->pm1b_ctlr_blk, slp_typa);

    if (fadt->pm1a_ctlr_blk != 0)
        outw((uint16_t)fadt->pm1a_ctlr_blk, value);
    if (fadt->pm1b_ctlr_blk != 0)
        outw((uint16_t)fadt->pm1b_ctlr_blk, value);

    /*
     * If we are still running, the power-off did not take effect; return and
     * let the caller fall back to halting.
     */
}
