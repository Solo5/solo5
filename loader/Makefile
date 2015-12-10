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

# use 32-bit cross compiler for loader
AS32=i686-elf-as
GCC32=i686-elf-gcc
LD32=i686-elf-gcc

CFLAGS32=-ffreestanding -std=gnu99 -O3 -Wall -Wextra -Werror
LDFLAGS32=-ffreestanding -O3 -nostdlib -lgcc 
LDLIBS32=-lgcc

all: loader

COBJS=\
loader.o \
page_tables.o \
gdt.o \
lib.o

# Put these in if doing any LOADER_DEBUG
KCOBJS=\
#	 serial_k.o \
#	 printk_k.o \


SOBJS=\
boot_s.o \
to64_s.o

%_s.o: %.s
	$(AS32) $< -o $@

%_k.o: ../kernel/%.c ../kernel/kernel.h
	$(GCC32) $(CFLAGS32) -c $< -o $@

%.o: %.c loader.h
	$(GCC32) $(CFLAGS32) -c $< -o $@

loader: Makefile loader.ld $(COBJS) $(KCOBJS) $(SOBJS) 
	$(LD32) -T loader.ld $(LDFLAGS32) $(COBJS) $(KCOBJS) $(SOBJS) -o loader $(LDLIBS32)

clean: 
	@rm -f *.o loader 
