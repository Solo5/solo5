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

include Makefile.common

COMMON_COBJS=\
abort.o \
cpu.o \
cpu_vectors.o \
ee_printf.o \
intr.o \
lib.o \
malloc.o \
stubs.o \
pvclock.o

UKVM_COBJS=\
ukvm/kernel.o \
ukvm/gdt.o \
ukvm/io.o \
ukvm/platform.o \
ukvm/platform_intr.o \
ukvm/mem.o \
ukvm/time.o \
$(COMMON_COBJS)

VIRTIO_COBJS=\
virtio/boot.o \
virtio/kernel.o \
virtio/platform.o \
virtio/platform_intr.o \
virtio/mem.o \
virtio/pci.o \
virtio/serial.o \
virtio/time.o \
virtio/virtio_net.o \
virtio/virtio_blk.o \
virtio/tscclock.o \
virtio/clock_subr.o \
$(COMMON_COBJS)

HEADERS=\
kernel.h

all: virtio ukvm

.PHONY: virtio ukvm
virtio: virtio/solo5.o 
ukvm: ukvm/solo5.o 

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S $(HEADERS)
	$(CC) $(CFLAGS) -DASM_FILE -c $< -o $@

virtio/solo5.o: $(VIRTIO_COBJS) virtio/solo5.lds
	$(LD) -r $(LDFLAGS) -o $@ $(VIRTIO_COBJS)
	$(OBJCOPY) -w -G solo5_\* -G _start $@ $@

ukvm/solo5.o: $(UKVM_COBJS) ukvm/solo5.lds
	$(LD) -r $(LDFLAGS) -o $@ $(UKVM_COBJS)
	$(OBJCOPY) -w -G solo5_\* -G _start $@ $@

.PHONY: clean
clean:
	$(RM) *.o virtio/*.o ukvm/*.o kernel 

# This target is for the top-level opam-install to get MD CFLAGS
.PHONY: print-md-cflags
print-md-cflags:
	@echo $(MD_CFLAGS)

.PHONY: print-ldflags
print-ldflags:
	@echo $(LDFLAGS)
