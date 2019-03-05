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

export TOPDIR := $(abspath .)
$(TOPDIR)/Makeconf:
	@echo CONFIGURE
	./configure.sh
include Makefile.common

SUBDIRS := bindings tenders/hvt tenders/spt tests

tests: bindings

.PHONY: $(SUBDIRS)

.PHONY: all
all: $(SUBDIRS)
.DEFAULT_GOAL := all

$(SUBDIRS):
	@echo "MAKE $@"
	$(MAKE) -C $@ $(MAKECMDGOALS) $(SUBOVERRIDE)

.PHONY: clean before-clean
# Ensure that a top-level "make clean" always cleans *all* possible build
# products and not some subset dependent on the setting of $(BUILD_*).
before-clean:
	$(eval export SUBOVERRIDE := BUILD_HVT=1 BUILD_SPT=1 BUILD_VIRTIO=1 BUILD_MUEN=1 BUILD_GENODE=1)
clean: before-clean $(SUBDIRS)
	@echo "CLEAN solo5"
	$(RM) solo5-bindings-virtio.pc
	$(RM) solo5-bindings-hvt.pc
	$(RM) solo5-bindings-muen.pc
	$(RM) solo5-bindings-genode.pc

.PHONY: clobber
clobber: MAKECMDGOALS := clean
clobber: clean
	@echo CLOBBER solo5
	-[ -d include/crt ] && $(RM) -r include/crt
	$(RM) Makeconf

.PHONY: force-install
install-opam-%: MAKECMDGOALS :=
install-opam-%: all solo5-bindings-%.pc force-install
	@echo INSTALL solo5
	@[ -d "$(PREFIX)" -a -d "$(PREFIX)/bin" ] || \
	    (echo "error: PREFIX not set or incorrect"; false)
	mkdir -p $(PREFIX)/lib/pkgconfig \
	    $(PREFIX)/lib/solo5-bindings-$* \
	    $(PREFIX)/include/solo5-bindings-$*/solo5 \
	    $(PREFIX)/include/solo5-bindings-$*/crt
	cp -R include/solo5/ include/crt/ $(PREFIX)/include/solo5-bindings-$*
	cp bindings/$*/solo5_$*.o bindings/$*/solo5_$*.lds \
	    $(PREFIX)/lib/solo5-bindings-$*
	cp solo5-bindings-$*.pc $(PREFIX)/lib/pkgconfig
ifdef BUILD_HVT
	cp tenders/hvt/solo5-hvt tenders/hvt/solo5-hvt-debug $(PREFIX)/bin
endif
ifdef BUILD_SPT
	cp tenders/spt/solo5-spt $(PREFIX)/bin
endif
ifdef BUILD_VIRTIO
	cp scripts/virtio-mkimage/solo5-virtio-mkimage.sh \
	    $(PREFIX)/bin/solo5-virtio-mkimage
	cp scripts/virtio-run/solo5-virtio-run.sh \
	    $(PREFIX)/bin/solo5-virtio-run
endif

.PHONY: force-uninstall
uninstall-opam-%: force-uninstall
	@echo UNINSTALL solo5
	@[ -d "$(PREFIX)" -a -d "$(PREFIX)/bin" ] || \
	    (echo "error: PREFIX not set or incorrect"; false)
	-[ -d "$(PREFIX)/include/solo5-bindings-$*/solo5" ] && \
	    $(RM) -r $(PREFIX)/include/solo5-bindings-$*/solo5
	-[ -d "$(PREFIX)/include/solo5-bindings-$*/crt" ] && \
	    $(RM) -r $(PREFIX)/include/solo5-bindings-$*/crt
	$(RM) $(PREFIX)/lib/solo5-bindings-$*/solo5_$*.o \
	    $(PREFIX)/lib/solo5-bindings-$*/solo5_$*.lds
	$(RM) $(PREFIX)/lib/pkgconfig/solo5-bindings-$*.pc
ifdef BUILD_HVT
	$(RM) $(PREFIX)/bin/solo5-hvt $(PREFIX)/bin/solo5-hvt-debug
endif
ifdef BUILD_SPT
	$(RM) $(PREFIX)/bin/solo5-spt
endif
ifdef BUILD_VIRTIO
	$(RM) $(PREFIX)/bin/solo5-virtio-mkimage
	$(RM) $(PREFIX)/bin/solo5-virtio-run
endif

# We want the MD CFLAGS, LDFLAGS and LD in the .pc file, where they can be
# picked up by the Mirage tool / other downstream consumers.
%.pc: %.pc.in
	sed <$< > $@ \
	    -e 's#!CFLAGS!#$(MD_CFLAGS)#g;' \
	    -e 's#!LDFLAGS!#$(LDFLAGS)#g;' \
	    -e 's#!GENODE_APP_LDFLAGS!#$(GENODE_APP_LDFLAGS)#g;' \
	    -e 's#!LD!#$(LD)#g;' \

.PRECIOUS: %.pc

$(V).SILENT:
