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
 * hvt_gdb_freebsd_x86_64.c: glue between the GDB server (at hvt_modules_gdb.c)
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

#include "hvt.h"
#include "hvt_freebsd.h"
#include "hvt_gdb_x86_64.h"
#include "hvt_gdb.h"

int hvt_gdb_supported(void)
{
    return -1;
}

int hvt_gdb_read_registers(struct hvt *hvt,
                            uint8_t *registers,
                            size_t *len)
{
    return -1;
}

int hvt_gdb_write_registers(struct hvt *hvt,
                             uint8_t *registers,
                             size_t len)
{
    return -1;
}

int hvt_gdb_enable_ss(struct hvt *hvt)
{
    return -1;
}

int hvt_gdb_disable_ss(struct hvt *hvt)
{
    return -1;
}

int hvt_gdb_read_last_signal(struct hvt *hvt, int *signal)
{
    return -1;
}

int hvt_gdb_add_breakpoint(struct hvt *hvt, uint32_t type,
                            hvt_gpa_t addr, size_t len)
{
    return -1;
}

int hvt_gdb_remove_breakpoint(struct hvt *hvt, uint32_t type,
                               hvt_gpa_t addr, size_t len)
{
    return -1;
}
