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

# use 64-bit cross compiler for kernel
AS64=x86_64-elf-as
GCC64=x86_64-elf-gcc
LD64=x86_64-elf-gcc
AR64=x86_64-elf-ar

CFLAGS64=-ffreestanding		\
	 -mcmodel=large		\
	 -mno-red-zone		\
	 -mno-3dnow		\
	 -std=gnu99 -O2		\
	 -Wall -Wextra -Werror  \
	 -D__SOLO5__
#-g 

LDFLAGS64=-ffreestanding -O2 -nostdlib -lgcc -z max-page-size=0x1000 -static -fno-PIC
LDLIBS64=-lgcc

all: kernel

SOLO5_DIR=$(abspath .)
SOLO5_H=$(SOLO5_DIR)/kernel.h

MODULE_CFLAGS=
MODULE_OBJS=
MODULE_HFILES=
MODULE_CLEAN=

MODULE_DIR=lib
#include $(MODULE_DIR)/Makefile.openlibm

APP_OBJS=app_undefined.o
APP_LIBS=
APP_CLEAN=
include Makefile.app

CFLAGS64 += $(MODULE_CFLAGS)

COBJS=\
$(MODULE_OBJS) \
$(APP_OBJS) \
ee_printf.o \
interrupts.o \
kernel.o \
lib.o \
malloc.o \
mem.o \
net.o \
pci.o \
printk.o \
serial.o \
time.o \
virtio.o

SOBJS=\
kernel_s.o \
mem_s.o \
interrupts_s.o

HEADERS=\
kernel.h \
../loader/multiboot.h \
../loader/loader_info.h

.PHONY: kernel.ld
kernel.ld: 
	bash gen_ld.bash > kernel.ld

interrupt_vectors.s: interrupts.h
	bash gen_interrupts.bash > interrupt_vectors.s

%_s.o: %.s interrupt_vectors.s
	$(AS64) $< -o $@

.PHONY: app_and_undef
app_and_undef:
	bash gen_app_and_undef.bash

%.o: %.c $(HEADERS)
	$(GCC64) $(CFLAGS64) -c $< -o $@

kernel: Makefile kernel.ld $(COBJS) $(SOBJS) 
	$(LD64) -T kernel.ld $(LDFLAGS64) -o $@ $(COBJS) $(SOBJS) $(LDLIBS64) $(APP_LIBS) 

kernel_clean:
	@rm -f *.o kernel 

clean: $(APP_CLEAN) $(MODULE_CLEAN) kernel_clean
