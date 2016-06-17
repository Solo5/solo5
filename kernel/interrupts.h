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

/* This file has a special format because it is not only included by
 * interrupts.c but also used to generate assembly stubs
 * (interrupt_vectors.s) for interrupt handling routines.  Edit at
 * your own risk!
 *
 * The format is pretty self-explanatory:
 *
 * #define name vector comment
 * 
 * The main gotcha is that the comment is mandatory and specifies
 * whether an error code is pushed onto the stack by the processor.
 * Any user defined interrupt will have no error code.
 */

/* processor exceptions: 0x0 - 0x13 */
#define INTR_EXCEPTION_DIVIDE_ERROR  0x00 /* no error code */
#define INTR_EXCEPTION_DEBUG         0x01 /* no error code */ 
#define INTR_EXCEPTION_NMI           0x02 /* no error code */ 
#define INTR_EXCEPTION_BREAKPOINT    0x03 /* no error code */ 
#define INTR_EXCEPTION_OVERFLOW      0x04 /* no error code */ 
#define INTR_EXCEPTION_BOUND         0x05 /* no error code */ 
#define INTR_EXCEPTION_INVALID_OP    0x06 /* no error code */ 
#define INTR_EXCEPTION_DEVICE        0x07 /* no error code */ 
#define INTR_EXCEPTION_DBL_FAULT     0x08 /* error code */ 
#define INTR_EXCEPTION_COPROC_SEG    0x09 /* no error code */
#define INTR_EXCEPTION_INVALID_TSS   0x0a /* error code */ 
#define INTR_EXCEPTION_SEGMENT       0x0b /* error code */ 
#define INTR_EXCEPTION_STACK         0x0c /* error code */ 
#define INTR_EXCEPTION_GPF           0x0d /* error code */ 
#define INTR_EXCEPTION_PF            0x0e /* error code */ 

#define INTR_EXCEPTION_FPU           0x10 /* no error code */
#define INTR_EXCEPTION_ALIGN         0x11 /* error code */
#define INTR_EXCEPTION_MACHINE_CHK   0x12 /* no error code */ 
#define INTR_EXCEPTION_SIMD_FP       0x13 /* no error code */ 
#define INTR_EXCEPTION_VIRT          0x14 /* no error code */ 

/* remapped irqs */
#define INTR_IRQ_0      0x20 /* no error code */
#define INTR_IRQ_1      0x21 /* no error code */
#define INTR_IRQ_2      0x22 /* no error code */
#define INTR_IRQ_3      0x23 /* no error code */
#define INTR_IRQ_4      0x24 /* no error code */
#define INTR_IRQ_5      0x25 /* no error code */
#define INTR_IRQ_6      0x26 /* no error code */
#define INTR_IRQ_7      0x27 /* no error code */
#define INTR_IRQ_8      0x28 /* no error code */
#define INTR_IRQ_9      0x29 /* no error code */
#define INTR_IRQ_10     0x2a /* no error code */
#define INTR_IRQ_11     0x2b /* no error code */
#define INTR_IRQ_12     0x2c /* no error code */
#define INTR_IRQ_13     0x2d /* no error code */
#define INTR_IRQ_14     0x2e /* no error code */
#define INTR_IRQ_15     0x2f /* no error code */

/* user defined interrupts: 0x31 - ... */
#define INTR_USER_1     0x31 /* no error code */
#define INTR_USER_TIMER 0x32 /* no error code */
