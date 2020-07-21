/*
 * Copyright (c) 2015-2020 Contributors as noted in the AUTHORS file
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

#include "../cpu_x86_64.h"
#include "xen/elfnote.h"

#define ENTRY(x) .text; .globl x; .type x,%function; x:
#define END(x)   .size x, . - x

/*
 * Tell Xen that we are a PVH-capable kernel.
 * See https://xenbits.xen.org/docs/unstable/misc/pvh.html.
 */
.section .note.solo5.xen, "a", @note

    .align 4
    .long  4
    .long  4
    .long  XEN_ELFNOTE_PHYS32_ENTRY
    .ascii "Xen\0"
    .long  _start32

.code32

/*
 * Xen PVH entry point.
 *
 * Xen puts us only in 32bit flat protected mode, and passes a pointer to
 * struct hvm_start_info in %ebx. It's our responsibility to install a page
 * table and switch to long mode.  Notably, we can't call C code until we've
 * switched to long mode.
 */
ENTRY(_start32)
    cld
    movl $bootstack, %esp

    /* Save Xen hvm_start_info pointer at top of stack, we pop it in 64bit */
    pushl $0
    pushl %ebx

    /*
     * Load bootstrap GDT and reload segment registers, with the exception of
     * CS, which will be reloaded on jumping to _start64
     */
    lgdt (gdt64_ptr)

    movl $0x10, %eax
    movl %eax, %ds
    movl %eax, %es
    movl %eax, %ss

    xorl %eax, %eax
    movl %eax, %fs
    movl %eax, %gs

    /*
     * x86_64 switch to long mode
     */

    /* 1: Enable PAE */
    movl %cr4, %eax
    orl $X86_CR4_PAE, %eax
    movl %eax, %cr4

    /* 2: Load PML4 pointer */
    movl $cpu_pml4, %eax
    movl %eax, %cr3

    /* 3: Request long mode enable and enable NX functionality */
    movl $0xc0000080, %ecx
    rdmsr
    orl $(X86_EFER_LME | X86_EFER_NXE), %eax
    wrmsr

    /* 4a: Enable paging and supervisor write protect */
    movl %cr0, %eax
    orl $(X86_CR0_PG | X86_CR0_WP), %eax

    /* 4b: CPU sets long mode enabled after this instruction */
    movl %eax, %cr0

    /* 5: Reload CS with a 64-bit selector */
    pushw $0x08
    pushl $_start64
    lret

    /* NOTREACHED */
    jmp haltme

haltme:
    cli
    hlt
    jmp haltme
END(_start32)

.code64

ENTRY(_start64)
    movq $bootstack, %rsp
    xorq %rbp, %rbp

    /* Enable FPU and SSE units */
    movq %cr0, %rax
    andq $(~X86_CR0_EM), %rax
    orq $(X86_CR0_MP | X86_CR0_NE), %rax
    movq %rax, %cr0
    movq %cr4, %rax
    orq $(X86_CR4_OSXMMEXCPT | X86_CR4_OSFXSR), %rax
    movq %rax, %cr4
    ldmxcsr (mxcsr_ptr)

    /* Read Xen hvm_start_info pointer */
    movq -8(%rsp), %rdi

    /* Call into C with correct start-of-day stack alignment */
    pushq $0x0
    pushq $0x0
    call _start

    /* NOTREACHED */
    jmp haltme
END(_start64)

/*
 * void _newstack(uint64_t stack_start, void (*tramp)(void *), void *arg);
 *
 * Switch to a new stack at (stack_start), and transfer control to
 * (*tramp)(arg).
 */
ENTRY(_newstack)
    movq %rdi, %rsp
    movq %rdx, %rdi

    /* As above, ensure correct start-of-day stack alignment */
    pushq $0x0
    pushq $0x0
    call *%rsi

    /* NOTREACHED */
    jmp haltme
END(_newstack)

.data

/*
 * amd64 programmer's manual:
 *
 * "In long mode, segmentation is not used ... except for a few exceptions."
 *
 * Uuuyea, exceptions.
 *
 * The GDT here is used only during bootstrapping, and is reloaded by
 * cpu_init().
 */

.align 64
gdt64:
    .quad 0x0                           /* 0: NULL selector  */
    .quad GDT_DESC_CODE_VAL             /* 0x8: 64-bit code  */
    .quad GDT_DESC_DATA_VAL             /* 0x10: 64-bit data */
gdt64_end:
.align 64

.type gdt64_ptr, @object
gdt64_ptr:
    .word gdt64_end-gdt64-1
    .quad gdt64

.type mxcsr_ptr, @object
mxcsr_ptr:
    .word 0x1f80                        /* Intel SDM power-on default */

#include "pagetable.S"

.section .bss

/*
 * Stack used during bootstrapping, reloaded before calling _start2().
 */
.space 4096
bootstack:
