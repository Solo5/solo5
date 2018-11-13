# Copyright (c) 2015-2018 Contributors as noted in the AUTHORS file
#
# This file is part of Solo5, a sandboxed execution environment.
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

SHM_COBJS=\
muen/channel.o \
muen/reader.o \
muen/shm_net.o \
muen/writer.o

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

COMMON_HVT_COBJS=\
hvt/start.o \
hvt/platform.o \
hvt/platform_intr.o \
hvt/time.o

HVT_COBJS=\
$(COMMON_HVT_COBJS) \
$(SHM_COBJS) \
hvt/platform_lifecycle.o \
hvt/yield.o \
hvt/tscclock.o \
hvt/console.o \
hvt/net.o \
hvt/block.o \
$(COMMON_COBJS)

VIRTIO_COBJS=\
virtio/boot.o \
virtio/start.o \
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
$(COMMON_HVT_COBJS) \
$(SHM_COBJS) \
muen/muen-block.o \
muen/muen-clock.o \
muen/muen-console.o \
muen/muen-net.o \
muen/muen-sinfo.o \
muen/muen-platform_lifecycle.o \
muen/muen-yield.o \
$(COMMON_COBJS)

GENODE_COBJS=\
genode/stubs.o

HEADERS=\
bindings.h

all: virtio hvt

.PHONY: virtio hvt muen genode
virtio: virtio/solo5_virtio.o
hvt: hvt/solo5_hvt.o
muen: muen/solo5_muen.o
genode: genode/solo5.lib.so

CFLAGS+=-D__SOLO5_BINDINGS__

ifeq ($(TARGET_ARCH), x86_64)
# Prevent the compiler from adding optimizations that use shared FPU state.
# This is necessary for early boot code and trap handlers.
MD_CFLAGS+=-mno-sse -mno-mmx -mno-aes -mno-avx
endif

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S $(HEADERS)
	$(CC) $(CFLAGS) -DASM_FILE -c $< -o $@

virtio/solo5_virtio.o: $(VIRTIO_COBJS) virtio/solo5_virtio.lds
	$(LD) -r $(LDFLAGS) -o $@ $(VIRTIO_COBJS)
	$(OBJCOPY) -w -G solo5_\* -G _start\* $@ $@

hvt/solo5_hvt.o: $(HVT_COBJS) hvt/solo5_hvt.lds
	$(LD) -r $(LDFLAGS) -o $@ $(HVT_COBJS)
	$(OBJCOPY) -w -G solo5_\* -G _start\* $@ $@

muen/solo5_muen.o: $(MUEN_COBJS) muen/solo5_muen.lds
	$(LD) -r $(LDFLAGS) -o $@ $(MUEN_COBJS)
	$(OBJCOPY) -w -G solo5_\* -G _start\* $@ $@

genode/solo5.lib.so: $(GENODE_COBJS)
	$(LD) $(GENODE_LIB_LDFLAGS) -o $@ $^

.PHONY: clean
clean:
	$(RM) *.o virtio/*.o hvt/*.o muen/*.o genode/*.o genode/*.lib.so
