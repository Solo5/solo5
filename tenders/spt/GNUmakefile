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

TOP?=../..
include $(TOP)/Makeconf

.PHONY: all clean
all: solo5-spt

CFLAGS=-Wall -Werror -std=c99 -O2 -g -I../../include/solo5
LDFLAGS=-Wl,-z -Wl,noexecstack $(SPT_EXTRA_LDFLAGS)
LDLIBS=-lseccomp

OBJS=spt_main.o spt_core.o spt_elf.o spt_launch_$(TARGET_ARCH).o \
     spt_module_net.o spt_module_block.o

# This needs to be re-defined manually, as GNU Make has the undocumented
# "feature" of using $(TARGET_ARCH) in the default implicit rule for %.c.
%.o: %.c
	@echo "CC $<"
	$(CC) -c $(CFLAGS) -o $@ $<

solo5-spt: $(OBJS)
	@echo "LINK $@"
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

clean:
	@echo "CLEAN"
	$(RM) $(OBJS) solo5-spt

$(V).SILENT:
