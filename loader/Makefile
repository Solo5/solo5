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
GCC_INSTALL_DIR=$(shell LANG=C gcc -m32 -print-search-dirs | sed -n -e 's/install: \(.*\)/\1/p')
MD_CFLAGS=-m32 -nostdinc -isystem $(GCC_INSTALL_DIR)include \
	  -ffreestanding -mno-red-zone -mno-3dnow
LDFLAGS=-melf_i386 -nostdlib -z max-page-size=0x1000 -static
LDLIBS=$(shell gcc -m32 -print-libgcc-file-name)
# Platform-independent CFLAGS (danger: -g breaks things)
CFLAGS=$(MD_CFLAGS) -std=gnu99 -Wall -Wextra -Werror -O3


# use 32-bit cross compiler for loader
# AS=i686-elf-as
# CC=i686-elf-gcc
# LD=i686-elf-gcc

#CFLAGS=-ffreestanding -std=gnu99 -O3 -Wall -Wextra -Werror
# LDFLAGS=-ffreestanding -O3 -nostdlib -lgcc 
# LDLIBS=-lgcc

all: loader

COBJS=\
loader.o \
page_tables.o \
gdt.o \
lib.o

# Put these in if doing any LOADER_DEBUG
KCOBJS=\
	 serial_k.o \
	 printk_k.o \


SOBJS=\
boot_s.o \
to64_s.o

%_s.o: %.s
	$(AS) --32 $< -o $@

%_k.o: ../kernel/%.c ../kernel/kernel.h
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c loader.h
	$(CC) $(CFLAGS) -c $< -o $@

loader: Makefile loader.ld $(COBJS) $(KCOBJS) $(SOBJS) 
	$(LD) -T loader.ld $(LDFLAGS) $(COBJS) $(KCOBJS) $(SOBJS) -o loader $(LDLIBS)

clean: 
	@rm -f *.o loader 
