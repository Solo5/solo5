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

SUBDIRS := elftool tenders toolchain bindings tests

bindings: toolchain

tests: bindings elftool

.PHONY: $(SUBDIRS)

.PHONY: build
ifdef CONFIG_DISABLE_TOOLCHAIN
build: elftool tenders
else
build: $(SUBDIRS)
endif
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

delete-werror:
	sed -i -e 's/-Werror//g' Makefile.common

# The following target should be used to generate a release tarball of Solo5.
# The tarball will be generated from the currently checked-out branch. Note that
# some files related to CI or E2E tests are deliberately not included, see
# .gitattributes for a list.
.PHONY: distrib
distrib: GIT_VERSION = $(shell git -C . describe --tags --always)
distrib: GIT_OLDBRANCH = $(shell git rev-parse --abbrev-ref HEAD)
distrib: distrib-gen-version-h delete-werror
	git checkout -b distrib/$(GIT_VERSION)
	git add $(VERSION_H).distrib
	git add Makefile.common
	git commit -m "Release tarball for $(GIT_VERSION)"
	@echo DISTRIB solo5-$(GIT_VERSION).tar.gz
	git archive --format=tar.gz --prefix=solo5-$(GIT_VERSION)/ \
	    distrib/$(GIT_VERSION) >solo5-$(GIT_VERSION).tar.gz
	GIT_VERSION=$(GIT_VERSION) ./scripts/opam-release.sh
	-git checkout $(GIT_OLDBRANCH)
	-git branch -D distrib/$(GIT_VERSION)
	$(RM) $(VERSION_H).distrib
	git checkout -- Makefile.common

$(SUBDIRS):
	@echo "MAKE $@"
	$(MAKE) -C $@ $(MAKECMDGOALS) $(SUBOVERRIDE)

.PHONY: clean
ifdef CONFIG_DISABLE_TOOLCHAIN
clean: elftool tenders
else
clean: $(SUBDIRS)
endif
	@echo "CLEAN solo5"
	$(RM) $(VERSION_H)

.PHONY: distclean
distclean: MAKECMDGOALS := clean
distclean: clean
	@echo DISTCLEAN solo5
	$(RM) Makeconf Makeconf.sh

DESTDIR ?=
PREFIX := $(CONFIG_PREFIX)
D := $(DESTDIR)$(PREFIX)
INSTALL := install -p

.PHONY: install-tools
install-tools: MAKECMDGOALS :=
install-tools: build
	@echo INSTALL tools
	mkdir -p $(D)/bin
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
	mkdir -p $(D)/include/solo5
	$(INSTALL) -m 0644 $(PUBLIC_HEADERS) $(D)/include/solo5

TOOLCHAIN_INCDIR := $(D)/include/$(CONFIG_TARGET_TRIPLE)
TOOLCHAIN_LIBDIR := $(D)/lib/$(CONFIG_TARGET_TRIPLE)

.PHONY: install-toolchain
install-toolchain: MAKECMDGOALS :=
install-toolchain: build
	@echo INSTALL toolchain
	mkdir -p $(D)/bin
	$(INSTALL) -m 0755 toolchain/bin/$(CONFIG_TARGET_TRIPLE)-cc $(D)/bin
	$(INSTALL) -m 0755 toolchain/bin/$(CONFIG_TARGET_TRIPLE)-ld $(D)/bin
	$(INSTALL) -m 0755 toolchain/bin/$(CONFIG_TARGET_TRIPLE)-objcopy $(D)/bin
	mkdir -p $(TOOLCHAIN_INCDIR) $(TOOLCHAIN_LIBDIR)
	cd toolchain/include/$(CONFIG_TARGET_TRIPLE) && \
	    find . -type d -exec mkdir -p "$(TOOLCHAIN_INCDIR)/{}" \; && \
	    find . -type f -name '*.h' -exec $(INSTALL) -m 0644 \
	    "{}" "$(TOOLCHAIN_INCDIR)/{}" \;
	$(INSTALL) -m 0644 bindings/solo5_stub.o $(TOOLCHAIN_LIBDIR)
	$(INSTALL) -m 0644 bindings/solo5_stub.lds $(TOOLCHAIN_LIBDIR)
ifdef CONFIG_HVT
	$(INSTALL) -m 0644 bindings/solo5_hvt.o $(TOOLCHAIN_LIBDIR)
	$(INSTALL) -m 0644 bindings/solo5_hvt.lds $(TOOLCHAIN_LIBDIR)
endif
ifdef CONFIG_SPT
	$(INSTALL) -m 0644 bindings/solo5_spt.o $(TOOLCHAIN_LIBDIR)
	$(INSTALL) -m 0644 bindings/solo5_spt.lds $(TOOLCHAIN_LIBDIR)
endif
ifdef CONFIG_VIRTIO
	$(INSTALL) -m 0644 bindings/solo5_virtio.o $(TOOLCHAIN_LIBDIR)
	$(INSTALL) -m 0644 bindings/solo5_virtio.lds $(TOOLCHAIN_LIBDIR)
endif
ifdef CONFIG_MUEN
	$(INSTALL) -m 0644 bindings/solo5_muen.o $(TOOLCHAIN_LIBDIR)
	$(INSTALL) -m 0644 bindings/solo5_muen.lds $(TOOLCHAIN_LIBDIR)
endif
ifdef CONFIG_XEN
	$(INSTALL) -m 0644 bindings/solo5_xen.o $(TOOLCHAIN_LIBDIR)
	$(INSTALL) -m 0644 bindings/solo5_xen.lds $(TOOLCHAIN_LIBDIR)
	cd include/xen && \
	    find . -type d -exec mkdir -p "$(D)/include/solo5/xen/{}" \;
	cd include/xen && \
	    find . -type f -name '*.h' -exec $(INSTALL) -m 0644 \
	    "{}" "$(D)/include/solo5/xen/{}" \;
endif

.PHONY: install-tenders
install-tenders: MAKECMDGOALS :=
install-tenders: build
	@echo INSTALL tenders
	mkdir -p $(D)/bin
ifdef CONFIG_HVT_TENDER
	$(INSTALL) tenders/hvt/solo5-hvt $(D)/bin
	- [ -f tenders/hvt/solo5-hvt-debug ] && \
	    $(INSTALL) tenders/hvt/solo5-hvt-debug $(D)/bin
endif
ifdef CONFIG_SPT_TENDER
	$(INSTALL) tenders/spt/solo5-spt $(D)/bin
endif

.PHONY: install
install: MAKECMDGOALS :=
ifdef CONFIG_DISABLE_TOOLCHAIN
install: install-tools install-tenders
else
install: install-tools install-tenders install-headers install-toolchain
endif

$(V).SILENT:
