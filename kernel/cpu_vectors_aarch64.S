/*
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a unikernel base layer.
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

/* Linkage for ARM */
#define __ALIGN .align 2
#define __ALIGN_STR ".align 2"

#define ALIGN __ALIGN
#define ALIGN_STR __ALIGN_STR

#define ENTRY(name)                             \
  .globl name;                                  \
  ALIGN;                                        \
  name:

#define GLOBAL(name)                            \
  .globl name;                                  \
  name:

#define END(name) \
  .size name, .-name

#define ENDPROC(name) \
  .type name, %function; \
  END(name)

/*
 * Exception trap types
 *-----------------
 */
#define ET_SYNC    0
#define ET_IRQ     1
#define ET_FIQ     2
#define ET_ERROR   3

/*
 * Invalid exception handlers
 */
.macro INVALID_TRAP, el, type
    b .
.endm

cpu_el0_sync_invalid:
    INVALID_TRAP 0, ET_SYNC
ENDPROC(cpu_el0_sync_invalid)

cpu_el0_irq_invalid:
    INVALID_TRAP 0, ET_IRQ
ENDPROC(cpu_el0_irq_invalid)

cpu_el0_fiq_invalid:
    INVALID_TRAP 0, ET_FIQ
ENDPROC(cpu_el0_fiq_invalid)

cpu_el0_error_invalid:
    INVALID_TRAP 0, ET_ERROR
ENDPROC(cpu_el0_error_invalid)

cpu_el1_sync_invalid:
    INVALID_TRAP 1, ET_SYNC
ENDPROC(cpu_el1_sync_invalid)

cpu_el1_irq_invalid:
    INVALID_TRAP 1, ET_IRQ
ENDPROC(cpu_el1_irq_invalid)

cpu_el1_fiq_invalid:
    INVALID_TRAP 1, ET_FIQ
ENDPROC(cpu_el1_fiq_invalid)

cpu_el1_error_invalid:
    INVALID_TRAP 1, ET_ERROR
ENDPROC(cpu_el1_error_invalid)

/*
 * Valid exception handlers
 */
.macro VALID_TRAP, el, type
    b .
.endm

cpu_el1_sync:
	VALID_TRAP 1, ET_SYNC
ENDPROC(cpu_el1_sync)

cpu_el1_irq:
	VALID_TRAP 1, ET_IRQ
ENDPROC(cpu_el1_irq)

cpu_el1_fiq:
	VALID_TRAP 1, ET_FIQ
ENDPROC(cpu_el1_fiq)

cpu_el1_error:
	VALID_TRAP 1, ET_ERROR
ENDPROC(cpu_el1_error)

.macro EXCEPTION_ENTRY label
    .align 7
    b \label
.endm

/*
 * Exception vectors.
 *
 * AArch64 unikernel runs in EL1 mode using the SP1 stack. The vectors
 * don't have a fixed address, only alignment (2^11) requirements.
 */
	.pushsection ".exception.text", "ax"

    .align  11
ENTRY(cpu_exception_vectors)
    /*
     * EL1 Exceptions with SP_EL0. We had configure the SPSel to
     * select the SP_EL1. So all such excetpions will be treated
     * as invalid.
     */
    EXCEPTION_ENTRY cpu_el1_sync_invalid        // Synchronous EL1t
    EXCEPTION_ENTRY cpu_el1_irq_invalid         // IRQ EL1t
    EXCEPTION_ENTRY cpu_el1_fiq_invalid         // FIQ EL1t
    EXCEPTION_ENTRY cpu_el1_error_invalid       // Error EL1t

    /* EL1 Exceptions with SP_EL1 */
    EXCEPTION_ENTRY cpu_el1_sync                // Synchronous EL1h
    EXCEPTION_ENTRY cpu_el1_irq                 // IRQ EL1h
    EXCEPTION_ENTRY cpu_el1_fiq                 // FIQ EL1h
    EXCEPTION_ENTRY cpu_el1_error               // Error EL1h

    /*
     * EL0 Exceptions, and EL0 execute state is AARCH64. We don't have
     * EL0 code, so any exception from EL0 will be treated as invalid
     */
    EXCEPTION_ENTRY cpu_el0_sync_invalid        // Synchronous 64-bit EL0
    EXCEPTION_ENTRY cpu_el0_irq_invalid         // IRQ 64-bit EL0
    EXCEPTION_ENTRY cpu_el0_fiq_invalid         // FIQ 64-bit EL0
    EXCEPTION_ENTRY cpu_el0_error_invalid       // Error 64-bit EL0

    /*
     * EL0 Exceptions, and EL0 execute state is AARCH32. We don't want to
     * make AARCH32 code to be compatible on AACH64, so all such exceptions
     * will be treated as invalid.
     */
    EXCEPTION_ENTRY cpu_el0_sync_invalid        // Synchronous 32-bit EL0
    EXCEPTION_ENTRY cpu_el0_irq_invalid         // IRQ 32-bit EL0
    EXCEPTION_ENTRY cpu_el0_fiq_invalid         // FIQ 32-bit EL0
    EXCEPTION_ENTRY cpu_el0_error_invalid       // Error 32-bit EL0
END(cpu_exception_vectors)
