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

        .text
        .code64
        .globl read_cr3
        .globl sse_enable
        
read_cr3:       
        mov %cr3, %rax
        ret

sse_enable:
        mov %cr0, %rax
        and $0xfffb, %ax       # clear CR0.EM
        or $0x2, %ax            # set CR0.MP
        mov %rax, %cr0

        mov %cr4, %rax
        or $(3<<9), %ax           # set CR4.OSFXSR and CR4.OSXMMEXCPT
        mov %rax, %cr4

        ret

        