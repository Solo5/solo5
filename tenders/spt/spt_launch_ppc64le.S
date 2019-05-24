/*
 * Copyright (c) 2019 Contributors as noted in the AUTHORS file
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

#define ENTRY(x) .text; .globl x; .type x,%function; x:
#define END(x)   .size x, . - x

/* TODO/XXX: This calling convention is probably not 100% correct */

/*
 * r3: stack pointer to use (0x1ffffff0)
 * r4: function to call     (0x102dd0)
 * r5: argument to pass to function (0x11000)
 */
ENTRY(spt_launch)
	mr %r1, %r3       // load stack pointer
	stdu %r1,-16(%r1) // adjust stack pointer a little more...
	mr %r3, %r5
	mtlr %r4          // move address of function to call into link register
	blr               // call function via link register
	nop
END(spt_launch)
