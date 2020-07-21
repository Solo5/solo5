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

export TOPDIR := $(abspath .)
$(TOPDIR)/Makeconf:
	$(error Makeconf not found, please run ./configure.sh)
include Makefile.common

SUBDIRS := bindings tenders elftool tests

tests: bindings elftool

.PHONY: $(SUBDIRS)

.PHONY: all
all: $(SUBDIRS)
.DEFAULT_GOAL := all

$(SUBDIRS): gen-version-h

.PHONY: gen-version-h distrib-gen-version-h

VERSION_H := include/solo5/solo5_version.h

# (re)generates a version.h on every invocation of "make".
# This will also get run before a "make [dist]clean", can't be helped.
gen-version-h:
	@echo "GEN $(VERSION_H)"
	scripts/gen_version_h.sh $(VERSION_H)

distrib-gen-version-h:
	@echo "GEN $(VERSION_H).distrib"
	scripts/gen_version_h.sh $(VERSION_H).distrib

# The following target should be used to generate a release tarball of Solo5.
# The tarball will be generated from the currently checked-out branch. Note that
# some files related to CI or E2E tests are deliberately not included, see
# .gitattributes for a list.
.PHONY: distrib
distrib: GIT_VERSION = $(shell git -C . describe --dirty --tags --always)
distrib: GIT_OLDBRANCH = $(shell git rev-parse --abbrev-ref HEAD)
distrib: distrib-gen-version-h
	git checkout -b distrib/$(GIT_VERSION)
	git add $(VERSION_H).distrib
	git commit -m "Release tarball for $(GIT_VERSION)"
	@echo DISTRIB solo5-$(GIT_VERSION).tar.gz
	git archive --format=tar.gz --prefix=solo5-$(GIT_VERSION)/ \
	    distrib/$(GIT_VERSION) >solo5-$(GIT_VERSION).tar.gz
	-git checkout $(GIT_OLDBRANCH)
	-git branch -D distrib/$(GIT_VERSION)
	$(RM) $(VERSION_H).distrib

$(SUBDIRS):
	@echo "MAKE $@"
	$(MAKE) -C $@ $(MAKECMDGOALS) $(SUBOVERRIDE)

.PHONY: clean before-clean
# Ensure that a top-level "make clean" always cleans *all* possible build
# products and not some subset dependent on the setting of $(BUILD_*).
before-clean:
	$(eval export SUBOVERRIDE := CONFIG_HVT=1 CONFIG_SPT=1 CONFIG_VIRTIO=1 CONFIG_MUEN=1 CONFIG_GENODE=1 CONFIG_XEN=1)
clean: before-clean $(SUBDIRS)
	@echo "CLEAN solo5"
	$(RM) opam/solo5-bindings-hvt.pc
	$(RM) opam/solo5-bindings-spt.pc
	$(RM) opam/solo5-bindings-virtio.pc
	$(RM) opam/solo5-bindings-muen.pc
	$(RM) opam/solo5-bindings-genode.pc
	$(RM) opam/solo5-bindings-xen.pc
	$(RM) $(VERSION_H)

.PHONY: distclean
distclean: MAKECMDGOALS := clean
distclean: clean
	@echo DISTCLEAN solo5
	-[ -d include/crt ] && $(RM) -r include/crt
	$(RM) Makeconf

.PHONY: force-install force-uninstall

install-tools: MAKECMDGOALS :=
install-tools: DESTDIR ?= /usr/local
install-tools: all force-install
	@echo INSTALL solo5
	mkdir -p $(DESTDIR)/bin
	cp elftool/solo5-elftool $(DESTDIR)/bin

uninstall-tools: DESTDIR ?= /usr/local
uninstall-tools: force-uninstall
	@echo UNINSTALL solo5
	$(RM) $(DESTDIR)/bin/solo5-elftool

install-opam-%: MAKECMDGOALS :=
install-opam-%: all opam/solo5-bindings-%.pc force-install
	@echo INSTALL solo5
	@[ -d "$(PREFIX)" -a -d "$(PREFIX)/bin" ] || \
	    (echo "error: PREFIX not set or incorrect"; false)
	mkdir -p $(PREFIX)/lib/pkgconfig \
	    $(PREFIX)/lib/solo5-bindings-$* \
	    $(PREFIX)/include/solo5-bindings-$*/solo5 \
	    $(PREFIX)/include/solo5-bindings-$*/crt
	cp -R include/solo5 include/crt $(PREFIX)/include/solo5-bindings-$*
ifndef CONFIG_GENODE
	cp bindings/$*/solo5_$*.o bindings/$*/solo5_$*.lds \
	    $(PREFIX)/lib/solo5-bindings-$*
else
	cp bindings/$*/solo5.lib.so bindings/$*/genode_dyn.ld \
	    $(PREFIX)/lib/solo5-bindings-$*
endif
	cp opam/solo5-bindings-$*.pc $(PREFIX)/lib/pkgconfig
	cp elftool/solo5-elftool $(PREFIX)/bin
ifdef CONFIG_HVT
	cp tenders/hvt/solo5-hvt tenders/hvt/solo5-hvt-configure $(PREFIX)/bin
	- [ -f tenders/hvt/solo5-hvt-debug ] && \
	    cp tenders/hvt/solo5-hvt-debug $(PREFIX)/bin
endif
ifdef CONFIG_SPT
	cp tenders/spt/solo5-spt $(PREFIX)/bin
endif
ifdef CONFIG_VIRTIO
	cp scripts/virtio-mkimage/solo5-virtio-mkimage.sh \
	    $(PREFIX)/bin/solo5-virtio-mkimage
	cp scripts/virtio-run/solo5-virtio-run.sh \
	    $(PREFIX)/bin/solo5-virtio-run
endif

# uninstall-opam-% may not have a Makeconf available, so should always uninstall
# all build products from all solo5-bindings variants regardless.
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
	$(RM) $(PREFIX)/bin/solo5-elftool
# CONFIG_HVT
	$(RM) $(PREFIX)/bin/solo5-hvt $(PREFIX)/bin/solo5-hvt-debug \
	    $(PREFIX)/bin/solo5-hvt-configure
# CONFIG_SPT
	$(RM) $(PREFIX)/bin/solo5-spt
# CONFIG_VIRTIO
	$(RM) $(PREFIX)/bin/solo5-virtio-mkimage
	$(RM) $(PREFIX)/bin/solo5-virtio-run
# CONFIG_GENODE
	$(RM) $(PREFIX)/lib/solo5-bindings-$*/solo5.lib.so \
	    $(PREFIX)/lib/solo5-bindings-$*/genode_dyn.ld

# The following targets are kept for backwards compatibility, as otherwise
# upgrading existing OPAM switches will fail. They should be removed at some
# point, along with the dummy solo5-hvt-configure.
opam-hvt-uninstall: uninstall-opam-hvt ;

opam-spt-uninstall: uninstall-opam-spt ;

opam-virtio-uninstall: uninstall-opam-virtio ;

opam-muen-uninstall: uninstall-opam-muen ;

opam-genode-uninstall: uninstall-opam-genode ;

$(V).SILENT:
