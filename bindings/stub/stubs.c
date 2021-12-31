/*
 * Copyright (c) 2015-2021 Contributors as noted in the AUTHORS file
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
 * stubs.c: Solo5 'stub' bindings. These are used by the toolchain to provide a
 * "default" for non-Solo5-aware build systems such as GNU autoconf, that use
 * 'cc' as a linker to test for the presence of symbols or otherwise produce
 * "executables" during build.
 */

#include "../bindings.h"
#include "elf_abi.h"

#define U __attribute__((unused))

void _start(const void *arg U)
{
    for(;;);
}

uintptr_t SSP_GUARD_SYMBOL;

void SSP_FAIL_SYMBOL(void)
{
    for(;;);
}

DECLARE_ELF_INTERP

void solo5_console_write(const char *buf U, size_t size U)
{
}

void solo5_exit(int status U) {
    for(;;);
}

void solo5_abort(void) {
    for(;;);
}

solo5_time_t solo5_clock_monotonic(void) {
    return ~0;
}

solo5_time_t solo5_clock_wall(void) {
    return ~0;
}

void solo5_yield(solo5_time_t deadline U, solo5_handle_set_t *ready_set U)
{
    return;
}

solo5_result_t solo5_net_acquire(const char *name U, solo5_handle_t *handle U,
        struct solo5_net_info *info U)
{
    return SOLO5_R_EUNSPEC;
}

solo5_result_t solo5_net_write(solo5_handle_t handle U, const uint8_t *buf U,
        size_t size U)
{
    return SOLO5_R_EUNSPEC;
}

solo5_result_t solo5_net_read(solo5_handle_t handle U, uint8_t *buf U,
        size_t size U, size_t *read_size U)
{
    return SOLO5_R_EUNSPEC;
}

solo5_result_t solo5_block_acquire(const char *name U, solo5_handle_t *handle U,
        struct solo5_block_info *info U)
{
    return SOLO5_R_EUNSPEC;
}

solo5_result_t solo5_block_write(solo5_handle_t handle U, solo5_off_t offset U,
        const uint8_t *buf U, size_t size U)
{
    return SOLO5_R_EUNSPEC;
}

solo5_result_t solo5_block_read(solo5_handle_t handle U, solo5_off_t offset U,
        uint8_t *buf U, size_t size U)
{
    return SOLO5_R_EUNSPEC;
}

solo5_result_t solo5_set_tls_base(uintptr_t base U)
{
    return SOLO5_R_EUNSPEC;
}
