# Copyright (c) 2015-2019 Contributors as noted in the AUTHORS file
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

ifneq ($(filter 1,$(CONFIG_HVT) $(CONFIG_SPT)),)

common_LIB := common/libcommon.a
common_SRCS := common/elf.c common/mft.c common/block_attach.c \
    common/tap_attach.c
common_OBJS := $(patsubst %.c,%.o,$(common_SRCS))

$(common_LIB): $(common_OBJS)
	@echo HOSTAR $@
	$(HOSTAR) rc $@ $^

all_TARGETS += $(common_LIB)

%.o: %.c %.d
	$(HOSTCOMPILE.c)

%.o: %.S %.d
	$(HOSTCOMPILE.S)

%.d: ;
.PRECIOUS: %.d

endif

ifdef CONFIG_HVT

hvt_SRCS := hvt/hvt_boot_info.c hvt/hvt_core.c hvt/hvt_main.c \
    hvt/hvt_cpu_$(CONFIG_ARCH).c
hvt_MODULES ?= blk net

ifeq ($(CONFIG_HOST), Linux)
    hvt_SRCS += hvt/hvt_kvm.c hvt/hvt_kvm_$(CONFIG_ARCH).c
    hvt_debug_MODULES ?= gdb dumpcore
    all_TARGETS += hvt/solo5-hvt hvt/solo5-hvt-debug

    HOSTLDFLAGS += -Wl,-z -Wl,noexecstack
else ifeq ($(CONFIG_HOST), FreeBSD)
    hvt_SRCS += hvt/hvt_freebsd.c hvt/hvt_freebsd_$(CONFIG_ARCH).c
    hvt_debug_MODULES ?= gdb dumpcore
    all_TARGETS += hvt/solo5-hvt hvt/solo5-hvt-debug
ifeq ($(CONFIG_HVT_FREEBSD_ENABLE_CAPSICUM), 1)
    HOSTLDLIBS += -lnv
    CFLAGS += -DDHVT_FREEBSD_ENABLE_CAPSICUM=1
endif
else ifeq ($(CONFIG_HOST), OpenBSD)
    hvt_SRCS += hvt/hvt_openbsd.c hvt/hvt_openbsd_$(CONFIG_ARCH).c
    all_TARGETS += hvt/solo5-hvt
endif

hvt_SRCS += $(patsubst %,hvt/hvt_module_%.c,$(hvt_MODULES))
hvt_debug_SRCS += $(patsubst %,hvt/hvt_module_%.c,$(hvt_debug_MODULES))
hvt_OBJS := $(patsubst %.c,%.o,$(hvt_SRCS))
hvt_debug_OBJS := $(patsubst %.c,%.o,$(hvt_debug_SRCS))

hvt/solo5-hvt: $(hvt_OBJS) $(common_LIB)
	$(HOSTLINK)

hvt/solo5-hvt-debug: $(hvt_OBJS) $(hvt_debug_OBJS) $(common_LIB)
	$(HOSTLINK)

endif # CONFIG_HVT

ifdef CONFIG_SPT

HOSTLDFLAGS += -Wl,-z -Wl,noexecstack

ifdef CONFIG_SPT_NO_PIE

HOSTLDFLAGS += -Wl,-Ttext-segment=0x40000000

endif

HOSTCFLAGS += $(MAKECONF_LIBSECCOMP_CFLAGS)
HOSTLDLIBS += $(MAKECONF_LIBSECCOMP_LDLIBS)

spt_SRCS := spt/spt_main.c spt/spt_core.c spt/spt_launch_$(CONFIG_ARCH).S \
    spt/spt_module_net.c spt/spt_module_block.c

spt_OBJS := $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(spt_SRCS)))

spt/solo5-spt: $(spt_OBJS) $(common_LIB)
	$(HOSTLINK)

all_TARGETS += spt/solo5-spt

endif # CONFIG_SPT

all: $(all_TARGETS)

all_OBJS := $(common_OBJS) $(hvt_OBJS) $(hvt_debug_OBJS) $(spt_OBJS)
all_DEPS := $(patsubst %.o,%.d,$(all_OBJS))

.PHONY: clean

clean:
	@echo "CLEAN tenders"
	$(RM) $(all_TARGETS) $(all_OBJS) $(all_DEPS)

include $(wildcard $(all_DEPS))
