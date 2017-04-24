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

.macro PUSH_CALLER_SAVE
    pushq %rax
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
.endm

.macro POP_CALLER_SAVE
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rax
.endm

.macro TRAP_ENTRY trapno, has_ec
.global cpu_trap_\trapno
.type cpu_trap_\trapno, @function
cpu_trap_\trapno:
    cld

.if \has_ec
    movq %cr2, %rdi                     /* for #PF */
    pushq %rdi
.else
    pushq $0                            /* no error code or %cr2, pass 0 */
    pushq $0
.endif
    PUSH_CALLER_SAVE

    movq $\trapno, %rdi
    movq %rsp, %rsi
    addq $72, %rsi                      /* struct trap_regs is at %rsp + 72 */
    call cpu_trap_handler

    POP_CALLER_SAVE
    addq $16, %rsp                      /* discard error code and %cr2 */

    iretq
.endm

.macro IRQ_ENTRY irqno
.global cpu_irq_\irqno
.type cpu_irq_\irqno, @function
cpu_irq_\irqno:
    cld

    PUSH_CALLER_SAVE

    movq $\irqno, %rdi
    call intr_irq_handler

    POP_CALLER_SAVE

    iretq
.endm

TRAP_ENTRY 0,  0 /* #DE */
TRAP_ENTRY 1,  0 /* #DB */
TRAP_ENTRY 2,  0 /* #NMI */
TRAP_ENTRY 3,  0 /* #BP */
TRAP_ENTRY 4,  0 /* #OF */
TRAP_ENTRY 5,  0 /* #BR */
TRAP_ENTRY 6,  0 /* #UD */
TRAP_ENTRY 7,  0 /* #NM */
TRAP_ENTRY 8,  1 /* #DF */
TRAP_ENTRY 10, 1 /* #TS */
TRAP_ENTRY 11, 1 /* #NP */
TRAP_ENTRY 12, 1 /* #SS */
TRAP_ENTRY 13, 1 /* #GP */
TRAP_ENTRY 14, 1 /* #PF */
TRAP_ENTRY 16, 0 /* #MF */
TRAP_ENTRY 17, 1 /* #AC */
TRAP_ENTRY 18, 0 /* #MC */
TRAP_ENTRY 19, 0 /* #XM */
TRAP_ENTRY 20, 0 /* #VE */

IRQ_ENTRY 0
IRQ_ENTRY 1
IRQ_ENTRY 2
IRQ_ENTRY 3
IRQ_ENTRY 4
IRQ_ENTRY 5
IRQ_ENTRY 6
IRQ_ENTRY 7
IRQ_ENTRY 8
IRQ_ENTRY 9
IRQ_ENTRY 10
IRQ_ENTRY 11
IRQ_ENTRY 12
IRQ_ENTRY 13
IRQ_ENTRY 14
IRQ_ENTRY 15
