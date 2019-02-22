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

hvt_SRCS := hvt_core.c hvt_elf.c hvt_main.c hvt_cpu_$(TENDER_ARCH).c

all_TARGETS = solo5-hvt
ifeq ($(TENDER_HOST), linux)
    hvt_SRCS += hvt_kvm.c hvt_kvm_$(TENDER_ARCH).c
    hvt_debug_MODULES ?= gdb dumpcore
    all_TARGETS += solo5-hvt-debug
else ifeq ($(TENDER_HOST), freebsd)
    hvt_SRCS += hvt_freebsd.c hvt_freebsd_$(TENDER_ARCH).c
    hvt_debug_MODULES ?= gdb dumpcore
    all_TARGETS += solo5-hvt-debug
else ifeq ($(TENDER_HOST), openbsd)
    hvt_SRCS += hvt_openbsd.c hvt_openbsd_$(TENDER_ARCH).c
endif

all: $(all_TARGETS)

CC := cc
CFLAGS := -Wall -Werror -std=c99 -O2 -g -I../../include/solo5

hvt_OBJS := $(patsubst %.c,%.o,$(hvt_SRCS))
hvt_OBJS += $(patsubst %,hvt_module_%.o,$(hvt_MODULES))
hvt_SRCS += $(patsubst %,hvt_module_%.c,$(hvt_MODULES))
hvt_MODULES_u := $(shell echo $(hvt_MODULES) | tr a-z A-Z)
hvt_MFLAGS := $(patsubst %,-DHVT_MODULE_%,$(hvt_MODULES_u))
hvt_debug_OBJS := $(addprefix _debug/, $(hvt_OBJS) $(patsubst %,hvt_module_%.o,$(hvt_debug_MODULES)))
hvt_debug_SRCS := $(hvt_SRCS) $(patsubst %,hvt_module_%.c,$(hvt_debug_MODULES))
hvt_debug_MODULES_u := $(shell echo $(hvt_debug_MODULES) | tr a-z A-Z)
hvt_debug_MFLAGS := $(hvt_MFLAGS) $(patsubst %,-DHVT_MODULE_%,$(hvt_debug_MODULES_u))

DEPDIR := .deps
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
debug_DEPDIR := .deps_debug
$(shell mkdir -p $(debug_DEPDIR) >/dev/null)
debug_DEPFLAGS = -MT $@ -MMD -MP -MF $(debug_DEPDIR)/$*.Td

.SUFFIXES:

%.o: %.c $(DEPDIR)/%.d
	@echo "CC $<"
	$(CC) $(DEPFLAGS) $(CFLAGS) $(hvt_MFLAGS) -c $< -o $@
	mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

solo5-hvt: $(hvt_OBJS)
	@echo "LINK $@"
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

_debug:
	mkdir -p _debug

_debug/%.o: %.c $(debug_DEPDIR)/%.d | _debug
	@echo "CC [debug] $<"
	$(CC) $(debug_DEPFLAGS) $(CFLAGS) $(hvt_debug_MFLAGS) -c $< -o $@
	@mv -f $(debug_DEPDIR)/$*.Td $(debug_DEPDIR)/$*.d && touch $@

solo5-hvt-debug: $(hvt_debug_OBJS)
	@echo "LINK $@"
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(DEPDIR)/%.d: ;
$(debug_DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d
.PRECIOUS: $(debug_DEPDIR)/%.d

clean:
	@echo "CLEAN"
	$(RM) solo5-hvt solo5-hvt-debug $(hvt_OBJS) $(hvt_debug_OBJS)
	$(RM) -r $(DEPDIR) $(debug_DEPDIR) _debug/

$(V).SILENT:

include $(wildcard $(patsubst %,$(DEPDIR)/%.d,$(basename $(hvt_SRCS))))
include $(wildcard $(patsubst %,$(debug_DEPDIR)/%.d,$(basename $(hvt_debug_SRCS))))
