# Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
#
# This file is part of Solo5, a unikernel base layer.
#
# Permission to use, copy, modify, and/or distribute this software
# for any purpose with or without fee is hereby granted, provided
# that the above copyright notice and this permission notice appear
# in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
# CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
# OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

include $(TOP)/Makefile.common

COMMON_COBJS=\
abort.o \
cpu_${TARGET_ARCH}.o \
cpu_vectors_${TARGET_ARCH}.o \
ee_printf.o \
intr.o \
lib.o \
mem.o \
exit.o \
log.o \
cmdline.o

COMMON_UKVM_COBJS=\
ukvm/kernel.o \
ukvm/platform.o \
ukvm/platform_intr.o \
ukvm/time.o

UKVM_COBJS=\
$(COMMON_UKVM_COBJS) \
ukvm/platform_lifecycle.o \
ukvm/yield.o \
ukvm/tscclock.o \
ukvm/console.o \
ukvm/net.o \
ukvm/block.o \
$(COMMON_COBJS)

VIRTIO_COBJS=\
virtio/boot.o \
virtio/kernel.o \
virtio/platform.o \
virtio/platform_intr.o \
virtio/pci.o \
virtio/serial.o \
virtio/time.o \
virtio/virtio_ring.o \
virtio/virtio_net.o \
virtio/virtio_blk.o \
virtio/tscclock.o \
virtio/clock_subr.o \
virtio/pvclock.o \
$(COMMON_COBJS)

MUEN_COBJS=\
$(COMMON_UKVM_COBJS) \
muen/channel.o \
muen/reader.o \
muen/writer.o \
muen/muen-block.o \
muen/muen-clock.o \
muen/muen-console.o \
muen/muen-net.o \
muen/muen-platform_lifecycle.o \
muen/muen-yield.o \
muen/muen-sinfo.o \
$(COMMON_COBJS)

HEADERS=\
kernel.h

all: virtio ukvm

.PHONY: virtio ukvm muen
virtio: virtio/solo5.o
ukvm: ukvm/solo5.o
muen: muen/solo5.o

CFLAGS+=-D__SOLO5_KERNEL__

ifeq ($(TARGET_ARCH), x86_64)
# Prevent the compiler from adding optimizations that use shared FPU state.
# This is necessary for early boot code and trap handlers.
MD_CFLAGS+=-mno-sse -mno-mmx -mno-aes -mno-avx
endif

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S $(HEADERS)
	$(CC) $(CFLAGS) -DASM_FILE -c $< -o $@

virtio/solo5.o: $(VIRTIO_COBJS) virtio/solo5.lds
	$(LD) -r $(LDFLAGS) -o $@ $(VIRTIO_COBJS)
	$(OBJCOPY) -w -G solo5_\* -G _start\* $@ $@

ukvm/solo5.o: $(UKVM_COBJS) ukvm/solo5.lds
	$(LD) -r $(LDFLAGS) -o $@ $(UKVM_COBJS)
	$(OBJCOPY) -w -G solo5_\* -G _start\* $@ $@

muen/solo5.o: $(MUEN_COBJS) ukvm/solo5.lds
	$(LD) -r $(LDFLAGS) -o $@ $(MUEN_COBJS)
	$(OBJCOPY) -w -G solo5_\* -G _start\* $@ $@

.PHONY: clean
clean:
	$(RM) *.o virtio/*.o ukvm/*.o muen/*.o kernel
