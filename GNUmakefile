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

.PHONY: build
build: $(SUBDIRS) $(PCFILES)
.DEFAULT_GOAL := build

$(SUBDIRS): gen-version-h

.PHONY: gen-version-h distrib-gen-version-h

VERSION_H := include/version.h

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
	$(RM) $(PCFILES) $(VERSION_H)

.PHONY: distclean
distclean: MAKECMDGOALS := clean
distclean: clean
	@echo DISTCLEAN solo5
	-[ -d include/crt ] && $(RM) -r include/crt
	$(RM) Makeconf

DESTDIR ?=
PREFIX ?= /usr/local
PCFILEDIR ?= /lib/pkgconfig
PCFILES := pkgconfig/solo5-bindings-hvt.pc \
    pkgconfig/solo5-bindings-spt.pc \
    pkgconfig/solo5-bindings-virtio.pc \
    pkgconfig/solo5-bindings-muen.pc \
    pkgconfig/solo5-bindings-genode.pc \
    pkgconfig/solo5-bindings-xen.pc
D := $(DESTDIR)$(PREFIX)
INSTALL := install -p

.PHONY: install-tools
install-tools: MAKECMDGOALS :=
install-tools: build
	@echo INSTALL tools
	$(INSTALL) -d $(D)/bin
	$(INSTALL) elftool/solo5-elftool $(D)/bin
	$(INSTALL) scripts/virtio-mkimage/solo5-virtio-mkimage.sh \
	    $(D)/bin/solo5-virtio-mkimage
	$(INSTALL) scripts/virtio-run/solo5-virtio-run.sh \
	    $(D)/bin/solo5-virtio-run

PUBLIC_HEADERS := include/elf_abi.h include/hvt_abi.h include/mft_abi.h \
    include/spt_abi.h include/solo5.h

.PHONY: install-headers
install-headers: MAKECMDGOALS :=
install-headers: build
	@echo INSTALL headers
	$(INSTALL) -d $(D)/include/solo5
	$(INSTALL) -m 0644 $(PUBLIC_HEADERS) $(D)/include/solo5
	cd include/crt && \
	    find . -type d -exec $(INSTALL) -d \
	    "$(D)/include/solo5/crt/{}" \;
	cd include/crt && \
	    find . -type f -name '*.h' -exec $(INSTALL) -m 0644 \
	    "{}" "$(D)/include/solo5/crt/{}" \;

.PHONY: install-bindings
install-bindings: MAKECMDGOALS :=
install-bindings: build
	@echo INSTALL bindings
	$(INSTALL) -d $(D)/lib/solo5
	$(INSTALL) -d $(D)$(PCFILEDIR)
ifdef CONFIG_HVT
	$(INSTALL) -m 0644 bindings/hvt/solo5_hvt.o $(D)/lib/solo5
	$(INSTALL) -m 0644 bindings/hvt/solo5_hvt.lds $(D)/lib/solo5
	$(INSTALL) -m 0644 pkgconfig/solo5-bindings-hvt.pc $(D)$(PCFILEDIR)
endif
ifdef CONFIG_SPT
	$(INSTALL) -m 0644 bindings/spt/solo5_spt.o $(D)/lib/solo5
	$(INSTALL) -m 0644 bindings/spt/solo5_spt.lds $(D)/lib/solo5
	$(INSTALL) -m 0644 pkgconfig/solo5-bindings-spt.pc $(D)$(PCFILEDIR)
endif
ifdef CONFIG_VIRTIO
	$(INSTALL) -m 0644 bindings/virtio/solo5_virtio.o $(D)/lib/solo5
	$(INSTALL) -m 0644 bindings/virtio/solo5_virtio.lds $(D)/lib/solo5
	$(INSTALL) -m 0644 pkgconfig/solo5-bindings-virtio.pc $(D)$(PCFILEDIR)
endif
ifdef CONFIG_MUEN
	$(INSTALL) -m 0644 bindings/muen/solo5_muen.o $(D)/lib/solo5
	$(INSTALL) -m 0644 bindings/muen/solo5_muen.lds $(D)/lib/solo5
	$(INSTALL) -m 0644 pkgconfig/solo5-bindings-muen.pc $(D)$(PCFILEDIR)
endif
ifdef CONFIG_GENODE
	$(INSTALL) -m 0644 bindings/genode/solo5.lib.so $(D)/lib/solo5
	$(INSTALL) -m 0644 bindings/genode/genode_dyn.ld $(D)/lib/solo5
	$(INSTALL) -m 0644 pkgconfig/solo5-bindings-genode.pc $(D)$(PCFILEDIR)
endif
ifdef CONFIG_XEN
	$(INSTALL) -m 0644 bindings/xen/solo5_xen.o $(D)/lib/solo5
	$(INSTALL) -m 0644 bindings/xen/solo5_xen.lds $(D)/lib/solo5
	$(INSTALL) -m 0644 pkgconfig/solo5-bindings-xen.pc $(D)$(PCFILEDIR)
	cd include/xen && \
	    find . -type d -exec $(INSTALL) -d \
	    "$(D)/include/solo5/xen/{}" \;
	cd include/xen && \
	    find . -type f -name '*.h' -exec $(INSTALL) -m 0644 \
	    "{}" "$(D)/include/solo5/xen/{}" \;
endif
install-bindings: $(PCFILES)

.PHONY: install-tenders
install-tenders: MAKECMDGOALS :=
install-tenders: build
	@echo INSTALL tenders
	$(INSTALL) -d $(D)/bin
ifdef CONFIG_HVT
	$(INSTALL) tenders/hvt/solo5-hvt $(D)/bin
	- [ -f tenders/hvt/solo5-hvt-debug ] && \
	    $(INSTALL) tenders/hvt/solo5-hvt-debug $(D)/bin
endif
ifdef CONFIG_SPT
	$(INSTALL) tenders/spt/solo5-spt $(D)/bin
endif

.PHONY: install
install: MAKECMDGOALS :=
install: install-headers install-tools install-bindings install-tenders

$(V).SILENT:
