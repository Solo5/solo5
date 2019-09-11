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

%.o: %.c %.d
	$(HOSTCOMPILE.c)

%.o: %.S %.d
	$(HOSTCOMPILE.S)

%.d: ;
.PRECIOUS: %.d

elftool_SRCS := elftool.c
elftool_OBJS := $(patsubst %.c,%.o,$(elftool_SRCS))

all_TARGETS += solo5-elftool

solo5-elftool: $(elftool_OBJS)
	$(HOSTLINK)

all: $(all_TARGETS)

all_OBJS := $(elftool_OBJS)
all_DEPS := $(patsubst %.o,%.d,$(all_OBJS))

.PHONY: clean

clean:
	@echo "CLEAN elftool"
	$(RM) $(all_TARGETS) $(all_OBJS) $(all_DEPS)

include $(wildcard $(all_DEPS))
