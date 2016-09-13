# Copyright (c) 2015, IBM
# Author(s): Dan Williams <djwillia@us.ibm.com>
#
# Permission to use, copy, modify, and/or distribute this software for
# any purpose with or without fee is hereby granted, provided that the
# above copyright notice and this permission notice appear in all
# copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
# DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
# OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

#define ENTRY(x) .text; .globl x; .type x,%function; x:
#define END(x)   .size x, . - x

.text
.code64

ENTRY(cpu_halt)
    cli
    hlt
    jmp cpu_halt
END(cpu_halt)

ENTRY(cpu_rdtsc)
    rdtsc
    shl $32, %rdx
    add %rdx, %rax
    ret
END(cpu_rdtsc)

ENTRY(cpu_idt_load)
    lidt 0(%rdi)
    ret
END(cpu_idt_load)

ENTRY(cpu_gdt_load)
    lgdt 0(%rdi)
    ret
END(cpu_gdt_load)

ENTRY(cpu_tss_load)
    ltr %di
    ret
END(cpu_tss_load)

ENTRY(cpu_sse_enable)
    mov %cr0, %rax
    and $0xfffb, %ax                    /* clear CR0.EM */
    or $0x2, %ax                        /* set CR0.MP */
    mov %rax, %cr0

    mov %cr4, %rax
    or $(3<<9), %ax                     /* set CR4.OSFXSR and CR4.OSXMMEXCPT */
    mov %rax, %cr4

    ret
END(cpu_sse_enable)
