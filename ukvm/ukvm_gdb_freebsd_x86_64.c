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

/*
 * ukvm_gdb_freebsd_x86_64.c: glue between the GDB server (at ukvm_modules_gdb.c)
 * and FreeBSD's vmm.
 */

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <machine/vmm.h>
#include <sys/param.h>
#include <sys/cpuset.h>
#include <machine/vmm_dev.h>

#include "ukvm.h"
#include "ukvm_hv_freebsd.h"
#include "ukvm_gdb_x86_64.h"
#include "ukvm_gdb.h"

int ukvm_gdb_supported(void)
{
    return -1;
}

int ukvm_gdb_read_registers(struct ukvm_hv *hv,
                            uint8_t *registers,
                            size_t *len)
{
    return -1;
}

int ukvm_gdb_write_registers(struct ukvm_hv *hv,
                             uint8_t *registers,
                             size_t len)
{
    return -1;
}

int ukvm_gdb_enable_ss(struct ukvm_hv *hv)
{
    return -1;
}

int ukvm_gdb_disable_ss(struct ukvm_hv *hv)
{
    return -1;
}

int ukvm_gdb_read_last_signal(struct ukvm_hv *hv, int *signal)
{
    return -1;
}

int ukvm_gdb_add_breakpoint(struct ukvm_hv *hv, uint32_t type,
                            ukvm_gpa_t addr, size_t len)
{
    return -1;
}

int ukvm_gdb_remove_breakpoint(struct ukvm_hv *hv, uint32_t type,
                               ukvm_gpa_t addr, size_t len)
{
    return -1;
}
