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

.PHONY: all clean

TENDER_HOST=linux
TENDER_ARCH=x86_64

hvt_MODULES ?= blk net

hvt_OBJS := hvt_core.o hvt_elf.o hvt_main.o hvt_cpu_$(TENDER_ARCH).o
hvt_HEADERS := hvt.h cc.h hvt_cpu_$(TENDER_ARCH).h

all_TARGETS = solo5-hvt
ifeq ($(TENDER_HOST), linux)
    hvt_OBJS += hvt_kvm.o hvt_kvm_$(TENDER_ARCH).o
    hvt_HEADERS += hvt_kvm.h
    hvt_debug_MODULES ?= gdb dumpcore
    all_TARGETS += solo5-hvt-debug
else ifeq ($(TENDER_HOST), freebsd)
    hvt_OBJS += hvt_freebsd.o hvt_freebsd_$(TENDER_ARCH).o
    hvt_HEADERS += hvt_freebsd.h
    hvt_debug_MODULES ?= gdb dumpcore
    all_TARGETS += solo5-hvt-debug
else ifeq ($(TENDER_HOST), openbsd)
    hvt_OBJS += hvt_openbsd.o hvt_openbsd_$(TENDER_ARCH).o
    hvt_HEADERS += hvt_openbsd.h
endif

all: $(all_TARGETS)

CC := cc
CFLAGS := -Wall -Werror -std=c99 -O2 -g -I../../include/solo5

hvt_OBJS += $(patsubst %,hvt_module_%.o,$(hvt_MODULES))
hvt_MODULES_u := $(shell echo $(hvt_MODULES) | tr a-z A-Z)
hvt_MFLAGS := $(patsubst %,-DHVT_MODULE_%,$(hvt_MODULES_u))
hvt_debug_OBJS := $(hvt_OBJS) $(patsubst %,hvt_module_%.o,$(hvt_debug_MODULES))
hvt_debug_MODULES_u := $(shell echo $(hvt_debug_MODULES) | tr a-z A-Z)
hvt_debug_MFLAGS := $(hvt_MFLAGS) $(patsubst %,-DHVT_MODULE_%,$(hvt_debug_MODULES_u))

%.o: %.c
	@echo "CC $<"
	$(CC) $(CFLAGS) $(hvt_MFLAGS) -c $< -o $@

solo5-hvt: $(hvt_OBJS)
	@echo "LINK $@"
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

_debug:
	mkdir -p _debug

_debug/%.o: %.c | _debug
	@echo "CC_debug $<"
	$(CC) $(CFLAGS) $(hvt_debug_MFLAGS) -c $< -o $@

solo5-hvt-debug: $(addprefix _debug/,$(hvt_debug_OBJS))
	@echo "LINK $@"
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	@echo "CLEAN"
	$(RM) solo5-hvt solo5-hvt-debug $(hvt_OBJS) $(addprefix _debug/,$(hvt_debug_OBJS))
	$(RM) -r _debug/

$(V).SILENT:
