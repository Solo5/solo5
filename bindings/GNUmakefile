# Copyright (c) 2015-2020 Contributors as noted in the AUTHORS file
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

ifndef TOPDIR
$(error TOPDIR must be set, run $(MAKE) from the top of the source tree or set it manually)
endif
include $(TOPDIR)/Makefile.common

.PHONY: all
all:
all_TARGETS :=

.SUFFIXES:
$(V).SILENT:

common_SRCS := cpu_$(CONFIG_TARGET_ARCH).c \
    cpu_vectors_$(CONFIG_TARGET_ARCH).S \
    abort.c crt.c printf.c intr.c lib.c mem.c exit.c log.c cmdline.c tls.c mft.c

common_hvt_SRCS := hvt/platform.c hvt/platform_intr.c

#
# Note that the module defining _start should be listed first in <target>_SRCS,
# in order to ensure that _start has a stable address (required for Muen).
#
stub_SRCS := stub/stubs.c

hvt_SRCS := hvt/start.c $(common_SRCS) $(common_hvt_SRCS) hvt/time.c \
    hvt/platform_lifecycle.c hvt/yield.c hvt/tscclock.c hvt/console.c \
    hvt/net.c hvt/block.c

spt_SRCS := spt/start.c \
    abort.c crt.c printf.c lib.c mem.c exit.c log.c cmdline.c tls.c mft.c \
    spt/bindings.c spt/block.c spt/net.c spt/platform.c \
    spt/sys_linux_$(CONFIG_TARGET_ARCH).c

virtio_SRCS := virtio/boot.S virtio/start.c $(common_SRCS) \
    virtio/platform.c virtio/platform_intr.c \
    virtio/pci.c virtio/serial.c virtio/time.c virtio/virtio_ring.c \
    virtio/virtio_net.c virtio/virtio_blk.c virtio/tscclock.c \
    virtio/clock_subr.c virtio/pvclock.c 

muen_SRCS := muen/start.c $(common_SRCS) $(common_hvt_SRCS) \
    muen/channel.c muen/reader.c muen/writer.c muen/muen-block.c \
    muen/muen-clock.c muen/muen-console.c muen/muen-net.c \
    muen/muen-platform_lifecycle.c muen/muen-yield.c muen/muen-sinfo.c

xen_SRCS := xen/boot.S xen/start.c $(common_SRCS) \
    xen/hypercall_page.S xen/console.c xen/platform.c xen/platform_intr.c \
    xen/evtchn.c xen/time.c xen/pvclock.c xen/stubs.c

CPPFLAGS+=-D__SOLO5_BINDINGS__
CPPFLAGS+=-ffile-prefix-map=${TOPDIR}=.

ifeq ($(CONFIG_TARGET_ARCH), x86_64)
# Prevent the compiler from adding optimizations that use shared FPU state.
# This is necessary for early boot code and trap handlers.
CFLAGS+=-mno-sse -mno-mmx -mno-aes -mno-avx -mno-red-zone
endif

%.o: %.c %.d
	$(COMPILE.c)

%.o: %.S %.d
	$(COMPILE.S)

%.d: ;
.PRECIOUS: %.d

all_TARGETS :=
all_OBJS :=
all_SRCS :=

ifdef CONFIG_TARGET_CC_IS_OPENBSD
    SSP_GUARD_SYMBOL=__guard_local
    SSP_FAIL_SYMBOL=__stack_smash_handler
else
    SSP_GUARD_SYMBOL=__stack_chk_guard
    SSP_FAIL_SYMBOL=__stack_chk_fail
endif
CFLAGS+=-DSSP_GUARD_SYMBOL=$(SSP_GUARD_SYMBOL)
CFLAGS+=-DSSP_FAIL_SYMBOL=$(SSP_FAIL_SYMBOL)

define LINK.bindings
	@echo "LD $@"
	$(LD) -r $(LDFLAGS) $^ -o $@
	@echo "OBJCOPY $@"
	$(OBJCOPY) -w -G solo5_\* -G _start\* \
	    -G $(SSP_GUARD_SYMBOL) -G $(SSP_FAIL_SYMBOL) \
	    $@ $@
endef

stub_OBJS := $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(stub_SRCS)))

solo5_stub.o: $(stub_OBJS)
	$(LINK.bindings)

all_TARGETS += solo5_stub.o
all_OBJS += $(stub_OBJS)
all_SRCS += $(stub_SRCS)

ifdef CONFIG_HVT
    hvt_OBJS := $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(hvt_SRCS)))

solo5_hvt.o: $(hvt_OBJS)
	$(LINK.bindings)

    all_TARGETS += solo5_hvt.o
    all_OBJS += $(hvt_OBJS)
    all_SRCS += $(hvt_SRCS)
endif

ifdef CONFIG_SPT
    spt_OBJS := $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(spt_SRCS)))

solo5_spt.o: $(spt_OBJS)
	$(LINK.bindings)

    all_TARGETS += solo5_spt.o
    all_OBJS += $(spt_OBJS)
    all_SRCS += $(spt_SRCS)
endif

ifdef CONFIG_VIRTIO
    virtio_OBJS := $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(virtio_SRCS)))

solo5_virtio.o: $(virtio_OBJS)
	$(LINK.bindings)

    all_TARGETS += solo5_virtio.o
    all_OBJS += $(virtio_OBJS)
    all_SRCS += $(virtio_SRCS)
endif

ifdef CONFIG_MUEN
    muen_OBJS := $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(muen_SRCS)))

solo5_muen.o: $(muen_OBJS)
	$(LINK.bindings)

    all_TARGETS += solo5_muen.o
    all_OBJS += $(muen_OBJS)
    all_SRCS += $(muen_SRCS)
endif

ifdef CONFIG_XEN
    xen_OBJS := $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(xen_SRCS)))

solo5_xen.o: $(xen_OBJS)
	$(LINK.bindings)

    all_TARGETS += solo5_xen.o
    all_OBJS += $(xen_OBJS)
    all_SRCS += $(xen_SRCS)

CPPFLAGS+=-D__XEN_INTERFACE_VERSION__=__XEN_LATEST_INTERFACE_VERSION__
endif

all: $(all_TARGETS)

all_DEPS := $(patsubst %.o,%.d,$(all_OBJS))

.PHONY: clean
clean:
	@echo "CLEAN bindings"
	$(RM) $(all_TARGETS) $(all_OBJS) $(all_DEPS)

include $(wildcard $(all_DEPS))
