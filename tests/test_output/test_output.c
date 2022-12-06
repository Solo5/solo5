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

#include "solo5.h"
#include "../../bindings/lib.c"

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

int solo5_app_main(const struct solo5_start_info *si __attribute__((unused)))
{
    char* str =
      "**** Solo5 standalone test_output **** (55 characters)\n"
      "**** Solo5 standalone test_output **** (111 characters)\n"
      "**** Solo5 standalone test_output **** (167 characters)\n"
      "**** Solo5 standalone test_output **** (223 characters)\n"
      "**** Solo5 standalone test_output **** (279 characters)\n"
      "**** Solo5 standalone test_output **** (335 characters)\n"
      "**** Solo5 standalone test_output **** (391 characters)\n"
      "**** Solo5 standalone test_output **** (447 characters)\n"
      "**** Solo5 standalone test_output **** (503 characters)\n"
      "**** Solo5 standalone test_output **** (559 characters)\n"
      "**** Solo5 standalone test_output **** (615 characters)\n"
      "**** Solo5 standalone test_output **** (671 characters)\n"
      "**** Solo5 standalone test_output **** (727 characters)\n"
      "**** Solo5 standalone test_output **** (783 characters)\n"
      "**** Solo5 standalone test_output **** (839 characters)\n"
      "**** Solo5 standalone test_output **** (895 characters)\n"
      "**** Solo5 standalone test_output **** (951 characters)\n"
      "**** Solo5 standalone test_output **** (1008 characters)\n"
      "**** Solo5 standalone test_output **** (1065 characters)\n"
      "**** Solo5 standalone test_output **** (1122 characters)\n"
      "**** Solo5 standalone test_output **** (1179 characters)\n"
      "**** Solo5 standalone test_output **** (1236 characters)\n"
      "**** Solo5 standalone test_output **** (1293 characters)\n"
      "**** Solo5 standalone test_output **** (1350 characters)\n"
      "**** Solo5 standalone test_output **** (1407 characters)\n"
      "**** Solo5 standalone test_output **** (1464 characters)\n"
      "**** Solo5 standalone test_output **** (1521 characters)\n"
      "**** Solo5 standalone test_output **** (1635 characters)\n"
      "**** Solo5 standalone test_output **** (1692 characters)\n"
      "**** Solo5 standalone test_output **** (1749 characters)\n"
      "**** Solo5 standalone test_output **** (1806 characters)\n"
      "**** Solo5 standalone test_output **** (1863 characters)\n"
      "**** Solo5 standalone test_output **** (1920 characters)\n"
      "**** Solo5 standalone test_output **** (1977 characters)\n"
      "**** Solo5 standalone test_output **** (2034 characters)\n"
      "**** Solo5 standalone test_output **** (2091 characters)\n"
      "**** Solo5 standalone test_output **** (2148 characters)\n"
      "**** Solo5 standalone test_output **** (2205 characters)\n"
      "**** Solo5 standalone test_output **** (2262 characters)\n"
      "**** Solo5 standalone test_output **** (2319 characters)\n"
      "**** Solo5 standalone test_output **** (2376 characters)\n"
      "**** Solo5 standalone test_output **** (2433 characters)\n"
      "**** Solo5 standalone test_output **** (2490 characters)\n"
      "**** Solo5 standalone test_output **** (2547 characters)\n"
      "**** Solo5 standalone test_output **** (2604 characters)\n"
      "**** Solo5 standalone test_output **** (2661 characters)\n"
      "**** Solo5 standalone test_output **** (2718 characters)\n"
      "**** Solo5 standalone test_output **** (2775 characters)\n"
      "**** Solo5 standalone test_output **** (2832 characters)\n"
      "**** Solo5 standalone test_output **** (2889 characters)\n"
      "**** Solo5 standalone test_output **** (2946 characters)\n"
      "**** Solo5 standalone test_output **** (3003 characters)\n"
      "**** Solo5 standalone test_output **** (3060 characters)\n"
      "**** Solo5 standalone test_output **** (3117 characters)\n"
      "**** Solo5 standalone test_output **** (3174 characters)\n"
      "**** Solo5 standalone test_output **** (3231 characters)\n"
      "**** Solo5 standalone test_output **** (3288 characters)\n"
      "**** Solo5 standalone test_output **** (3345 characters)\n"
      "**** Solo5 standalone test_output **** (3402 characters)\n"
      "**** Solo5 standalone test_output **** (3459 characters)\n"
      "**** Solo5 standalone test_output **** (3516 characters)\n"
      "**** Solo5 standalone test_output **** (3573 characters)\n"
      "**** Solo5 standalone test_output **** (3630 characters)\n"
      "**** Solo5 standalone test_output **** (3687 characters)\n"
      "**** Solo5 standalone test_output **** (3744 characters)\n"
      "**** Solo5 standalone test_output **** (3801 characters)\n"
      "**** Solo5 standalone test_output **** (3858 characters)\n"
      "**** Solo5 standalone test_output **** (3915 characters)\n"
      "**** Solo5 standalone test_output **** (3972 characters)\n"
      "**** Solo5 standalone test_output **** (4029 characters)\n"
      "**** Solo5 standalone test_output **** (4086 characters)\n"
      "**** Solo5 standalone test_output **** (4143 characters)\n"
      ;
    puts(str);
    return SOLO5_EXIT_SUCCESS;
}
