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

include Makefile.solo5

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

S5_CFLAGS += $(MODULE_CFLAGS)

COBJS=\
$(MODULE_OBJS) \
$(APP_OBJS) \
ee_printf.o \
gdt.o \
interrupts.o \
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

interrupt_vectors.s: interrupts.h
	bash gen_interrupts.bash > interrupt_vectors.s

%_s.o: %.s interrupt_vectors.s
	$(S5_AS) $< -o $@

.PHONY: app_and_undef
app_and_undef:
	bash gen_app_and_undef.bash

%.o: %.c $(HEADERS)
	$(S5_GCC) $(S5_CFLAGS) -c $< -o $@



kernel: Makefile kernel.ld $(COBJS) $(SOBJS) 
	$(S5_LD) -T kernel.ld $(LDFLAGS64) -o $@ $(COBJS) $(SOBJS) $(LDLIBS64) $(APP_LIBS) 

kernel_clean:
	@rm -f *.o kernel 

clean: $(APP_CLEAN) $(MODULE_CLEAN) kernel_clean
