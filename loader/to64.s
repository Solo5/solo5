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

    .set CR4_PAE_FLAG, 1<<5
    .set CR0_PG_BIT, 1<<31
    
    .set MSR_EFER, 0xC0000080
    .set MSR_EFER_LM_BIT, 1<<8

        /* These offsets must match those in gdt.c */
    .set GDT_CS_OFF, 0x08       # cs is first desc in GDT (after NULL)
    .set GDT_TSS_OFF, 0x18      # tss is third desc in GDT
        
    .globl to64_prep_paging
    .globl to64_prep_gdt
    .globl to64_prep_tss
    .globl to64_jump

    .text

to64_prep_paging:      
    /* args: [4(%esp) pml4 base] */
    mov 4(%esp), %eax           /* pml4 base arg */
    
    /* set pml4 base */
    mov %eax, %cr3

    /* enable pae */
    mov %cr4, %eax
    orl $CR4_PAE_FLAG, %eax
    mov %eax, %cr4
    
    /* enable long mode (compat) */
    mov $MSR_EFER, %ecx
    rdmsr
    orl $MSR_EFER_LM_BIT, %eax  
    wrmsr

    /* enable paging */
    mov %cr0, %eax              
    orl $CR0_PG_BIT, %eax
    mov %eax, %cr0              

    ret

/* prepare gdt for 64 bit mode */
to64_prep_gdt:
        mov 4(%esp), %eax           /* gdtptr argument */
        lgdt 0(%eax)
        ret

/* prepare TSS for 64 bit mode */        
to64_prep_tss:  
        mov 4(%esp), %eax           /* TSS offset in GDT */
        ltr %ax
        ret
        
to64_jump:      
    mov 4(%esp), %eax           /* entry point */
    mov 8(%esp), %edi           /* argument */
    ljmp $GDT_CS_OFF, $long_mode64

    .code64
long_mode64:
    mov 12(%esp), %rsp          /* move stack to the top of mem */
    jmp *%rax
