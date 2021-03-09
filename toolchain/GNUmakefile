# Copyright (c) 2015-2021 Contributors as noted in the AUTHORS file
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
all_TARGETS := bin/$(CONFIG_TARGET_TRIPLE)-cc \
    bin/$(CONFIG_TARGET_TRIPLE)-ld \
    bin/$(CONFIG_TARGET_TRIPLE)-objcopy \
    include/solo5 \
    include/$(CONFIG_TARGET_TRIPLE) \
    lib/$(CONFIG_TARGET_TRIPLE)

.SUFFIXES:
$(V).SILENT:

bin:
	mkdir -p $@

bin/$(CONFIG_TARGET_TRIPLE)-%: %.in | bin
	@echo "SUBST $@"
	sed -e 's!@@CONFIG_TARGET_ARCH@@!$(CONFIG_TARGET_ARCH)!g' \
	    -e 's!@@CONFIG_TARGET_TRIPLE@@!$(CONFIG_TARGET_TRIPLE)!g' \
	    -e 's!@@CONFIG_TARGET_CC@@!$(CONFIG_TARGET_CC)!g' \
	    -e 's!@@CONFIG_TARGET_CC_CFLAGS@@!$(CONFIG_TARGET_CC_CFLAGS)!g' \
	    -e 's!@@CONFIG_TARGET_CC_LDFLAGS@@!$(CONFIG_TARGET_CC_LDFLAGS)!g' \
	    -e 's!@@CONFIG_TARGET_LD@@!$(CONFIG_TARGET_LD)!g' \
	    -e 's!@@CONFIG_TARGET_LD_LDFLAGS@@!$(CONFIG_TARGET_LD_LDFLAGS)!g' \
	    -e 's!@@CONFIG_TARGET_LD_MAX_PAGE_SIZE@@!$(CONFIG_TARGET_LD_MAX_PAGE_SIZE)!g' \
	    -e 's!@@CONFIG_TARGET_OBJCOPY@@!$(CONFIG_TARGET_OBJCOPY)!g' \
	    $< >$@
	chmod +x $@

include:
	mkdir -p $@

include/solo5: | include
	ln -s $(TOPDIR)/include $@

include/$(CONFIG_TARGET_TRIPLE): | include
	@echo "GEN $@"
	./gen-headers.sh $@

lib:
	mkdir -p $@

lib/$(CONFIG_TARGET_TRIPLE): | lib
	ln -s $(TOPDIR)/bindings $@

all: $(all_TARGETS)

.PHONY: clean
clean:
	@echo "CLEAN toolchain"
	$(RM) -r bin/ include/ lib/
