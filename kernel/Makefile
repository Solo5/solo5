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
OBJCOPY=objcopy
# Ensure no host headers or libraries are used when building, with the
# exception of:
#   - Standard C headers required by C89/C99. -nostdinc removes access to these
#     so get it back by asking GCC for its installation directory.
#   - The static libgcc.a (compiler runtime) installed by the host GCC.
GCC_INSTALL_DIR=$(shell LANG=C $(CC) -print-search-dirs | sed -n -e 's/install: \(.*\)/\1/p')
MD_CFLAGS=-nostdinc -isystem $(GCC_INSTALL_DIR)include \
	  -ffreestanding -mno-red-zone -mno-3dnow
LDFLAGS=-nostdlib -z max-page-size=0x1000 -static
LDLIBS=$(shell $(CC) -print-libgcc-file-name)
# Platform-independent CFLAGS
CFLAGS=$(MD_CFLAGS) -std=gnu99 -Wall -Wextra -Werror -O2 -g

COMMON_COBJS=\
ee_printf.o \
interrupts.o \
lib.o \
malloc.o \
stubs.o \
pvclock.o

UKVM_COBJS=\
$(COMMON_COBJS) \
ukvm/gdt.o \
ukvm/io.o \
ukvm/kernel.o \
ukvm/low_level.o \
ukvm/low_level_interrupts.o \
ukvm/mem.o \
ukvm/time.o

VIRTIO_COBJS=\
$(COMMON_COBJS) \
virtio/boot.o \
virtio/low_level.o \
virtio/low_level_interrupts.o \
virtio/kernel.o \
virtio/mem.o \
virtio/net.o \
virtio/pci.o \
virtio/serial.o \
virtio/time.o \
virtio/virtio.o \
virtio/tscclock.o \
virtio/clock_subr.o

COBJS=$(COMMON_COBJS) $(VIRTIO_COBJS)
SOBJS=\
kernel_s.o \
mem_s.o \
interrupts_s.o

HEADERS=\
kernel.h

VIRTIO_TESTS=test_hello.virtio test_ping_serve.virtio
UKVM_TESTS=test_hello.ukvm 
TESTS=$(VIRTIO_TESTS) $(UKVM_TESTS)

all: virtio ukvm

.PHONY: virtio ukvm
virtio: virtio/solo5.o $(VIRTIO_TESTS)
ukvm: ukvm/solo5.o $(UKVM_TESTS)

interrupt_vectors.s: interrupts.h
	bash gen_interrupts.bash > interrupt_vectors.s

%_s.o: %.s interrupt_vectors.s
	$(AS) $< -o $@

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S $(HEADERS)
	$(CC) $(CFLAGS) -DASM_FILE -c $< -o $@

%.ukvm: %.o ukvm/solo5.o ukvm/solo5.lds
	$(LD) -T ukvm/solo5.lds $(LDFLAGS) -o $@ ukvm/solo5.o $< $(LDLIBS)

%.virtio: %.o virtio/solo5.o virtio/solo5.lds
	$(LD) -T virtio/solo5.lds $(LDFLAGS) -o $@ virtio/solo5.o $< $(LDLIBS)

virtio/solo5.o: $(VIRTIO_COBJS) $(SOBJS) virtio/solo5.lds
	$(LD) -r $(LDFLAGS) -o $@ $(VIRTIO_COBJS) $(SOBJS) 
	$(OBJCOPY) -w -G solo5_\* -G _start $@ $@

ukvm/solo5.o: $(UKVM_COBJS) $(SOBJS) ukvm/solo5.lds
	$(LD) -r $(LDFLAGS) -o $@ $(UKVM_COBJS) $(SOBJS) 
	$(OBJCOPY) -w -G solo5_\* -G kernel_main $@ $@

.PHONY: clean
clean:
	$(RM) *.o virtio/*.o ukvm/*.o kernel $(TESTS)

# This target is for the top-level opam-install to get MD CFLAGS
.PHONY: print-md-cflags
print-md-cflags:
	@echo $(MD_CFLAGS)

.PHONY: print-ldflags
print-ldflags:
	@echo $(LDFLAGS)

.PHONY: print-ldlibs
print-ldlibs:
	@echo $(LDLIBS)
