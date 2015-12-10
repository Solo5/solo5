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
    .globl kernel_hang
    .globl kernel_wait
    .globl kernel_waitloop
    .globl kernel_busyloop
    .globl rdtsc
    
kernel_hang:
	cli
	hlt
	jmp kernel_hang

kernel_wait:
    hlt
    ret

kernel_waitloop:
    sti
    hlt
    jmp kernel_wait

kernel_busyloop:
    cli
    jmp kernel_busyloop
        
        
rdtsc:
    rdtsc
    shl $32, %rdx
    add %rdx, %rax
    ret

