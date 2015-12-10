/* Copyright (c) 2015, IBM 
 * Author(s): Dan Williams <djwillia@us.ibm.com> 
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

#ifndef __LOADER_INFO_H__
#define __LOADER_INFO_H__
/*
 * This file is intended to include any constants or structures that
 * the loader wants to communicate to the kernel.  The loader has
 * already set up the page tables and the GDT/TSS, so rather than
 * duplicating constants (or rediscovering them), this is an attempt
 * to have them in one place.  
 */


/* alignment macros */
#define ALIGN_4K __attribute__((aligned(0x1000)))
#define ALIGN_64_BIT __attribute__((aligned(0x8)))

/* memory defines */
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define PAGE_MASK ~(0xfff)


/* We have already set up the GDT for the kernel.  Here are the
 * descriptor numbers (useful for when the kernel sets up the IDT) */
#define GDT_NUM_ENTRIES 5
#define GDT_DESC_NULL 0
#define GDT_DESC_CODE 1
#define GDT_DESC_DATA 2
#define GDT_DESC_TSS_LO  3
#define GDT_DESC_TSS_HI  4
#define GDT_DESC_TSS  GDT_DESC_TSS_LO
#define GDT_DESC_OFFSET(n) ((n) * 0x8)

/* We have already loaded a "known good" stack in the TSS */
#define TSS_IST_INDEX 0x1

#endif
