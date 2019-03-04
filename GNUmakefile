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
	$(RM) -r include/crt
	$(RM) Makeconf

PREFIX?=/nonexistent # Fail if not run from OPAM
OPAM_BINDIR=$(PREFIX)/bin
OPAM_HVT_LIBDIR=$(PREFIX)/lib/solo5-bindings-hvt
OPAM_HVT_INCDIR=$(PREFIX)/include/solo5-bindings-hvt
OPAM_VIRTIO_LIBDIR=$(PREFIX)/lib/solo5-bindings-virtio
OPAM_VIRTIO_INCDIR=$(PREFIX)/include/solo5-bindings-virtio
OPAM_MUEN_LIBDIR=$(PREFIX)/lib/solo5-bindings-muen
OPAM_MUEN_INCDIR=$(PREFIX)/include/solo5-bindings-muen
OPAM_GENODE_LIBDIR=$(PREFIX)/lib/solo5-bindings-genode
OPAM_GENODE_INCDIR=$(PREFIX)/include/solo5-bindings-genode
OPAM_SPT_LIBDIR=$(PREFIX)/lib/solo5-bindings-spt
OPAM_SPT_INCDIR=$(PREFIX)/include/solo5-bindings-spt

# We want the MD CFLAGS, LDFLAGS and LD in the .pc file, where they can be
# picked up by the Mirage tool / other downstream consumers.
%.pc: %.pc.in
	sed <$< > $@ \
	    -e 's#!CFLAGS!#$(MD_CFLAGS)#g;' \
	    -e 's#!LDFLAGS!#$(LDFLAGS)#g;' \
	    -e 's#!GENODE_APP_LDFLAGS!#$(GENODE_APP_LDFLAGS)#g;' \
	    -e 's#!LD!#$(LD)#g;' \

.PHONY: opam-virtio-install
opam-virtio-install: solo5-bindings-virtio.pc virtio
	mkdir -p $(OPAM_VIRTIO_INCDIR) $(OPAM_VIRTIO_LIBDIR)
	cp -R include/. $(OPAM_VIRTIO_INCDIR)
	cp bindings/virtio/solo5_virtio.o bindings/virtio/solo5_virtio.lds $(OPAM_VIRTIO_LIBDIR)
	mkdir -p $(OPAM_BINDIR)
	cp scripts/virtio-mkimage/solo5-virtio-mkimage.sh ${OPAM_BINDIR}/solo5-virtio-mkimage
	cp scripts/virtio-run/solo5-virtio-run.sh ${OPAM_BINDIR}/solo5-virtio-run
	mkdir -p $(PREFIX)/lib/pkgconfig
	cp solo5-bindings-virtio.pc $(PREFIX)/lib/pkgconfig

.PHONY: opam-virtio-uninstall
opam-virtio-uninstall:
	$(RM) -r $(OPAM_VIRTIO_INCDIR) $(OPAM_VIRTIO_LIBDIR)
	$(RM) $(PREFIX)/lib/pkgconfig/solo5-bindings-virtio.pc
	$(RM) ${OPAM_BINDIR}/solo5-mkimage
	$(RM) ${OPAM_BINDIR}/solo5-run-virtio

.PHONY: opam-hvt-install
opam-hvt-install: solo5-bindings-hvt.pc hvt
	mkdir -p $(OPAM_HVT_INCDIR) $(OPAM_HVT_LIBDIR)
	cp -R include/. $(OPAM_HVT_INCDIR)
	cp bindings/hvt/solo5_hvt.o bindings/hvt/solo5_hvt.lds $(OPAM_HVT_LIBDIR)
	mkdir -p $(OPAM_HVT_LIBDIR)/src
	cp -R tenders/hvt/*.[ch] include/solo5/hvt_abi.h $(OPAM_HVT_LIBDIR)/src
	mkdir -p $(OPAM_BINDIR)
	cp tenders/hvt/solo5-hvt-configure $(OPAM_BINDIR)
	mkdir -p $(PREFIX)/lib/pkgconfig
	cp solo5-bindings-hvt.pc $(PREFIX)/lib/pkgconfig

.PHONY: opam-hvt-uninstall
opam-hvt-uninstall:
	$(RM) -r $(OPAM_HVT_INCDIR) $(OPAM_HVT_LIBDIR)
	$(RM) $(OPAM_BINDIR)/solo5-hvt-configure
	$(RM) $(PREFIX)/lib/pkgconfig/solo5-bindings-hvt.pc

.PHONY: opam-muen-install
opam-muen-install: solo5-bindings-muen.pc muen
	mkdir -p $(OPAM_MUEN_INCDIR) $(OPAM_MUEN_LIBDIR)
	cp -R include/. $(OPAM_MUEN_INCDIR)
	cp bindings/muen/solo5_muen.o bindings/muen/solo5_muen.lds $(OPAM_MUEN_LIBDIR)
	mkdir -p $(PREFIX)/lib/pkgconfig
	cp solo5-bindings-muen.pc $(PREFIX)/lib/pkgconfig

.PHONY: opam-muen-uninstall
opam-muen-uninstall:
	$(RM) -r $(OPAM_MUEN_INCDIR) $(OPAM_MUEN_LIBDIR)
	$(RM) $(PREFIX)/lib/pkgconfig/solo5-bindings-muen.pc

.PHONY: opam-genode-install
opam-genode-install: solo5-bindings-genode.pc genode
	mkdir -p $(OPAM_GENODE_INCDIR) $(OPAM_GENODE_LIBDIR)
	cp -R include/. $(OPAM_GENODE_INCDIR)
	cp bindings/genode/solo5.lib.so bindings/genode/genode_dyn.ld $(OPAM_GENODE_LIBDIR)
	mkdir -p $(PREFIX)/lib/pkgconfig
	cp solo5-bindings-genode.pc $(PREFIX)/lib/pkgconfig

.PHONY: opam-genode-uninstall
opam-genode-uninstall:
	$(RM) -r $(OPAM_GENODE_INCDIR) $(OPAM_GENODE_LIBDIR)
	$(RM) $(PREFIX)/lib/pkgconfig/solo5-bindings-genode.pc

.PHONY: opam-spt-install
opam-spt-install: solo5-bindings-spt.pc spt
	mkdir -p $(OPAM_SPT_INCDIR) $(OPAM_SPT_LIBDIR)
	cp -R include/. $(OPAM_SPT_INCDIR)
	cp bindings/spt/solo5_spt.o bindings/spt/solo5_spt.lds $(OPAM_SPT_LIBDIR)
	mkdir -p $(PREFIX)/lib/pkgconfig
	cp solo5-bindings-spt.pc $(PREFIX)/lib/pkgconfig
	mkdir -p $(OPAM_BINDIR)
	cp tenders/spt/solo5-spt ${OPAM_BINDIR}/solo5-spt

.PHONY: opam-spt-uninstall
opam-spt-uninstall:
	$(RM) -r $(OPAM_SPT_INCDIR) $(OPAM_SPT_LIBDIR)
	$(RM) $(PREFIX)/lib/pkgconfig/solo5-bindings-spt.pc
	$(RM) ${OPAM_BINDIR}/solo5-spt

$(V).SILENT:
