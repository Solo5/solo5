/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2020, Takayuki Imada <takayuki.imada@gmail.com>
 */

unsigned __aeabi_read_tp(void);

unsigned __aeabi_read_tp(void)
{
    unsigned ret = 0x0U;
    __asm__ __volatile__("mrc p15, 0, %0, c13, c0, 2" :"=r"(ret)::);

    return ret;
}
