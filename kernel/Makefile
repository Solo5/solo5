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

AS=as
CC=gcc
LD=ld
# Ensure no host headers or libraries are used when building, with the
# exception of:
#   - Standard C headers required by C89/C99. -nostdinc removes access to these
#     so get it back by asking GCC for its installation directory.
#   - The static libgcc.a (compiler runtime) installed by the host GCC.
GCC_INSTALL_DIR=$(shell LANG=C gcc -print-search-dirs | sed -n -e 's/install: \(.*\)/\1/p')
MD_CFLAGS=-nostdinc -isystem $(GCC_INSTALL_DIR)include \
	  -ffreestanding -mno-red-zone -mno-3dnow
LDFLAGS=-nostdlib -z max-page-size=0x1000 -static
LDLIBS=$(shell gcc -print-libgcc-file-name)
# Platform-independent CFLAGS
CFLAGS=$(MD_CFLAGS) -std=gnu99 -Wall -Wextra -Werror -O2 -g

COBJS=\
ee_printf.o \
gdt.o \
interrupts.o \
io.o \
kernel.o \
lib.o \
malloc.o \
mem.o \
stubs.o \
time.o

SOBJS=\
kernel_s.o \
mem_s.o \
interrupts_s.o

HEADERS=\
kernel.h \
ukvm.h

TESTS=test_hello.bin

all: solo5.o $(TESTS)

interrupt_vectors.s: interrupts.h
	bash gen_interrupts.bash > interrupt_vectors.s

%_s.o: %.s interrupt_vectors.s
	$(AS) $< -o $@

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

%.bin: %.o solo5.o solo5.lds
	$(LD) -T solo5.lds $(LDFLAGS) -o $@ solo5.o $< $(LDLIBS)

solo5.o: $(COBJS) $(SOBJS)
	$(LD) -r -o $@ $(COBJS) $(SOBJS)

.PHONY: clean
clean:
	$(RM) *.o interrupt_vectors.s $(TESTS)
