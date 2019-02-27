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

include $(TOPDIR)/Makeconf

.PHONY: all

all:

TENDER_ARCH=x86_64

CFLAGS=-Wall -Werror -std=c99 -O2 -g -I../../include/solo5
LDFLAGS=-Wl,-z -Wl,noexecstack $(SPT_EXTRA_LDFLAGS)
LDLIBS=-lseccomp

spt_SRCS := spt_main.c spt_core.c spt_elf.c spt_launch_$(TENDER_ARCH).S \
     spt_module_net.c spt_module_block.c

spt_OBJS := $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(spt_SRCS)))

DEPFLAGS = -MT $@ -MMD -MP -MF $(@D)/$*.Td

.SUFFIXES:

%.o: %.c %.d
	@echo "CC $<"
	$(CC) $(DEPFLAGS) $(CFLAGS) -c $< -o $@
	mv -f $*.Td $*.d && touch $@

%.o: %.S %.d
	@echo "AS $<"
	$(CC) $(DEPFLAGS) $(CFLAGS) -DASM_FILE -c $< -o $@
	mv -f $*.Td $*.d && touch $@

%.d: ;
.PRECIOUS: %.d

all_TARGETS :=

solo5-spt: $(spt_OBJS)
	@echo "LINK $@"
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

all_TARGETS += solo5-spt

.PHONY: all

all: $(all_TARGETS)

all_OBJS := $(spt_OBJS)
all_DEPS := $(patsubst %.o,%.d,$(all_OBJS))

.PHONY: clean

clean:
	@echo "CLEAN tenders/hvt"
	$(RM) $(all_TARGETS) $(all_OBJS) $(all_DEPS)

$(V).SILENT:

include $(wildcard $(all_DEPS))
