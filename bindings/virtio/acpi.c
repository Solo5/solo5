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

/* This is not a full ACPI implementation.

   A full ACPI implementation involves a turing incomplete byte-code interpreter
   of third party code with full access to the system.  This is controversial,
   to say the least.  This implementation may be just enough to shut down a
   running system.
   */

#include "bindings.h"

bool acpi_detected = false;

/* This was adapted from kaworu's example in
https://forum.osdev.org/viewtopic.php?t=16990 */

#define byte uint8_t
#define word uint16_t
#define dword uint32_t

dword SMI_CMD;
byte ACPI_ENABLE;
byte ACPI_DISABLE;
dword PM1a_CNT;
dword PM1b_CNT;
word SLP_TYPa;
word SLP_TYPb;
word SLP_EN;
word SCI_EN;
byte PM1_CNT_LEN;

struct RSDPtr {
   byte Signature[8];
   byte CheckSum;
   byte OemID[6];
   byte Revision;
   dword *RsdtAddress;
};

struct FACP {
   byte Signature[4];
   dword Length;
   byte unneded1[40 - 8];
   dword *DSDT;
   byte unneded2[48 - 44];
   dword SMI_CMD;
   byte ACPI_ENABLE;
   byte ACPI_DISABLE;
   byte unneded3[64 - 54];
   dword PM1a_CNT_BLK;
   dword PM1b_CNT_BLK;
   byte unneded4[89 - 72];
   byte PM1_CNT_LEN;
};

/* check if the given address has a valid header */
static unsigned int *acpi_check_rsdptr(unsigned int *ptr)
{
    char *sig = "RSD PTR ";
    struct RSDPtr *rsdp = (struct RSDPtr *) ptr;
    byte *bptr;
    byte check = 0;
    uint32_t i;

    if (memcmp(sig, rsdp, 8) == 0) {
        // check checksum rsdpd
        bptr = (byte *) ptr;
        for (i=0; i<sizeof(struct RSDPtr); i++) {
            check += *bptr;
            bptr++;
        }
        // found valid rsdpd
        if (check == 0) {
            return (unsigned int *) rsdp->RsdtAddress;
        }
    }
    return NULL;
}

/* finds the acpi header and returns the address of the rsdt */
static unsigned int *acpi_get_rsdptr(void)
{
    uint32_t *addr;
    uint32_t *rsdp;

    /* search below the 1mb mark for RSDP signature */
    for (addr = (uint32_t *) 0x000E0000; addr < (uint32_t *)0x00100000; addr += 0x10/sizeof(addr)) {
        rsdp = acpi_check_rsdptr(addr);
        if (rsdp != NULL)
            return rsdp;
    }
    /* at address 0x40:0x0E is the RM segment of the ebda */
    uint32_t *ebda = (uint32_t *)((0x40E * 0x10) & 0x000FFFFF);   // transform segment into linear address

    /* search Extended BIOS Data Area for the Root System Description Pointer signature */
    for (addr = ebda; addr < (uint32_t *)ebda+1024; addr+= 0x10/sizeof(addr)) {
        rsdp = acpi_check_rsdptr(addr);
        if (rsdp != NULL)
            return rsdp;
    }
    return NULL;
}

/* checks for a given header and validates checksum */
static int acpi_check_header(uint32_t *ptr, char *sig)
{
   if (memcmp(ptr, sig, 4) == 0) {
      char *checkPtr = (char *)ptr;
      int len = *(ptr + 1);
      char check = 0;
      while (0 < len--) {
         check += *checkPtr;
         checkPtr++;
      }
      if (check == 0)
         return 0;
   }
   return -1;
}

static int acpi_enable(void)
{
    log(INFO, "Solo5: acpi_enable\n");
    if ((inw(PM1a_CNT) & SCI_EN) == 0) {
        /* check if acpi can be enabled */
        if (SMI_CMD != 0 && ACPI_ENABLE != 0) {
            outb(SMI_CMD, ACPI_ENABLE); /* send acpi enable command */
            /* give 3 seconds time to enable acpi */
            cpu_wasteful_milli_sleep(3000);
            int i = 0;
            if (PM1b_CNT != 0) {
                for (; i<300; i++ ) {
                  if ( (inw((unsigned int)PM1b_CNT) &SCI_EN) == 1 )
                      break;
                  cpu_wasteful_milli_sleep(10);
                }
            }
            if (i<300) {
                log(INFO, "Solo5: enabled ACPI\n");
                return 0;
            } else {
                log(INFO, "Solo5: timed out trying to enable ACPI\n");
                return -1;
            }
        } else {
            log(INFO, "Solo5: no known way to enable ACPI.\n");
            return -1;
          }
    } else {
        log(INFO, "Solo5: ACPI was already enabled\n");
        return 0;
    }
}


//
// bytecode of the \_S5 object
// -----------------------------------------
//        | (optional) |    |    |    |
// NameOP | \          | _  | S  | 5  | _
// 08     | 5A         | 5F | 53 | 35 | 5F
//
// -----------------------------------------------------------------------------------------------------------
//           |           |              | ( SLP_TYPa   ) | ( SLP_TYPb   ) | ( Reserved   ) | (Reserved    )
// PackageOP | PkgLength | NumElements  | byteprefix Num | byteprefix Num | byteprefix Num | byteprefix Num
// 12        | 0A        | 04           | 0A         05  | 0A          05 | 0A         05  | 0A         05
//
//----this-structure-was-also-seen----------------------
// PackageOP | PkgLength | NumElements |
// 12        | 06        | 04          | 00 00 00 00
//
// (Pkglength bit 6-7 encode additional PkgLength bytes [shouldn't be the case here])
//
static int acpi_init(void)
{
    log(INFO, "Solo5: acpi_init()\n");
    uint32_t *ptr = acpi_get_rsdptr();
    int entrys;

    if (ptr == NULL) {
        log(WARN, "Solo5: no rsdptr\n");
        return -1;
    }
    if (acpi_check_header(ptr, "RSDT") != 0) {
        log(WARN, "Solo5: RSDT not found\n");
        return -1;
    }
    // the RSDT contains an unknown number of pointers to acpi tables
    entrys = *(ptr + 1);
    entrys = (entrys-36) /4;
    ptr += 36/4;   // skip header information
    log(INFO, "Solo5: %d ACPI table entries\n", entrys);
    while (0<entrys--)
    {
        // check if the desired table is reached
        log(WARN, "Solo5: ACPI ptr first char: %c\n", *((char *)ptr));
        uint32_t *pptr = (uint32_t *)((uintptr_t)(*ptr));
        if (acpi_check_header(pptr, "FACP") == 0) {
          entrys = -2;
          struct FACP *facp = (struct FACP *)pptr;
          if (acpi_check_header((unsigned int *) facp->DSDT, "DSDT") == 0)
          {
              // search the \_S5 package in the DSDT
              char *S5Addr = (char *) facp->DSDT +36; // skip header
              int dsdtLength = *(facp->DSDT+1) -36;
              while (0 < dsdtLength--) {
                if (memcmp(S5Addr, "_S5_", 4) == 0)
                    break;
                S5Addr++;
              }
              // check if \_S5 was found
              if (dsdtLength > 0)
              {
                // check for valid AML structure
                if ( (*(S5Addr-1) == 0x08
                      || ( *(S5Addr-2) == 0x08 && *(S5Addr-1) == '\\') )
                    && *(S5Addr+4) == 0x12 ) {
                    S5Addr += 5;
                    S5Addr += ((*S5Addr &0xC0)>>6) +2;   // calculate PkgLength size

                    if (*S5Addr == 0x0A)
                      S5Addr++;   // skip byteprefix
                    SLP_TYPa = *(S5Addr)<<10;
                    S5Addr++;

                    if (*S5Addr == 0x0A)
                      S5Addr++;   // skip byteprefix
                    SLP_TYPb = *(S5Addr)<<10;

                    SMI_CMD = facp->SMI_CMD;

                    ACPI_ENABLE = facp->ACPI_ENABLE;
                    ACPI_DISABLE = facp->ACPI_DISABLE;

                    PM1a_CNT = facp->PM1a_CNT_BLK;
                    PM1b_CNT = facp->PM1b_CNT_BLK;

                    PM1_CNT_LEN = facp->PM1_CNT_LEN;

                    SLP_EN = 1<<13;
                    SCI_EN = 1;

                    return 0;
                } else {
                    log(WARN, "Solo5: \\_S5 parse error.\n");
                }
              } else {
                log(WARN, "Solo5: \\_S5 not present.\n");
              }
          } else {
              log(WARN, "Solo5: DSDT invalid.\n");
          }
        }
        ptr++;
    }
    log(INFO, "Solo5: no valid FACP present.\n");
    return -1;
}

void acpi_poweroff(void)
{
    log(INFO, "Solo5: ACPI poweroff initiated\n");
    if (!acpi_detected) {
        log(WARN, "Solo5: acpi_poweroff() called but ACPI poweroff is not available?\n");
        return;
    }
    log(INFO, "Solo5: init ACPI\n");
    acpi_init();
    /* SCI_EN is set to 1 if acpi shutdown is possible */
    if (SCI_EN == 0) {
        log(WARN, "Solo5: ACPI shutdown is not possible (SCI_EN == 0)\n");
        return;
    }
    log(WARN, "Solo5: initiating ACPI poweroff\n");
    acpi_enable();
    /* send the shutdown command */
    outw(PM1a_CNT, SLP_TYPa | SLP_EN );
    if (PM1b_CNT != 0)
      outw(PM1b_CNT, SLP_TYPb | SLP_EN);

    log(WARN, "Solo5: ACPI poweroff failed.\n");
}
