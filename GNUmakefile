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

export TOP=$(abspath .)
$(TOP)/Makeconf:
	./configure.sh
include Makefile.common

.PHONY: all
all: vt virtio muen
.DEFAULT_GOAL := all
.NOTPARALLEL: vt virtio muen

.PHONY: virtio
virtio:
ifeq ($(BUILD_VIRTIO), yes)
	$(MAKE) -C bindings virtio
	$(MAKE) -C tests virtio
endif

.PHONY: vt 
vt:
ifeq ($(BUILD_VT), yes)
	$(MAKE) -C bindings vt
	$(MAKE) -C tenders/vt
	$(MAKE) -C tests vt
endif

.PHONY: muen
muen:
ifeq ($(BUILD_MUEN), yes)
	$(MAKE) -C bindings muen
	$(MAKE) -C tests muen
endif

.PHONY: clean
clean:
	$(MAKE) -C bindings clean
ifeq ($(BUILD_VT), yes)
	$(MAKE) -C tenders/vt clean
endif
	$(MAKE) -C tests clean
	$(RM) solo5-bindings-virtio.pc
	$(RM) solo5-bindings-vt.pc
	$(RM) solo5-bindings-muen.pc
	$(RM) -r include/crt
	$(RM) Makeconf

PREFIX?=/nonexistent # Fail if not run from OPAM
OPAM_BINDIR=$(PREFIX)/bin
OPAM_VT_LIBDIR=$(PREFIX)/lib/solo5-bindings-vt
OPAM_VT_INCDIR=$(PREFIX)/include/solo5-bindings-vt/include
OPAM_VIRTIO_LIBDIR=$(PREFIX)/lib/solo5-bindings-virtio
OPAM_VIRTIO_INCDIR=$(PREFIX)/include/solo5-bindings-virtio/include
OPAM_MUEN_LIBDIR=$(PREFIX)/lib/solo5-bindings-muen
OPAM_MUEN_INCDIR=$(PREFIX)/include/solo5-bindings-muen/include

# We want the MD CFLAGS, LDFLAGS and LD in the .pc file, where they can be
# picked up by the Mirage tool / other downstream consumers.
%.pc: %.pc.in
	sed <$< > $@ \
	    -e 's#!CFLAGS!#$(MD_CFLAGS)#g;' \
	    -e 's#!LDFLAGS!#$(LDFLAGS)#g;' \
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
	rm -rf $(OPAM_VIRTIO_INCDIR) $(OPAM_VIRTIO_LIBDIR)
	rm -f $(PREFIX)/lib/pkgconfig/solo5-bindings-virtio.pc
	rm -f ${OPAM_BINDIR}/solo5-virtio-mkimage
	rm -f ${OPAM_BINDIR}/solo5-virtio-run

.PHONY: opam-vt-install
opam-vt-install: solo5-bindings-vt.pc vt
	mkdir -p $(OPAM_VT_INCDIR) $(OPAM_VT_LIBDIR)
	cp -R include/. $(OPAM_VT_INCDIR)
	cp bindings/vt/solo5_vt.o bindings/vt/solo5_vt.lds $(OPAM_VT_LIBDIR)
	mkdir -p $(OPAM_VT_LIBDIR)/solo5-vt-src
	cp -R tenders/vt/*.[ch] $(OPAM_VT_LIBDIR)/solo5-vt-src
	mkdir -p $(OPAM_BINDIR)
	cp tenders/vt/solo5-vt-configure $(OPAM_BINDIR)
	mkdir -p $(PREFIX)/lib/pkgconfig
	cp solo5-bindings-vt.pc $(PREFIX)/lib/pkgconfig

.PHONY: opam-vt-uninstall
opam-vt-uninstall:
	rm -rf $(OPAM_VT_INCDIR) $(OPAM_VT_LIBDIR)
	rm -f $(OPAM_BINDIR)/solo5-vt-configure
	rm -f $(PREFIX)/lib/pkgconfig/solo5-bindings-vt.pc

.PHONY: opam-muen-install
opam-muen-install: solo5-bindings-muen.pc muen
	mkdir -p $(OPAM_MUEN_INCDIR) $(OPAM_MUEN_LIBDIR)
	cp -R include/. $(OPAM_MUEN_INCDIR)
	cp bindings/muen/solo5_muen.o bindings/muen/solo5_muen.lds $(OPAM_MUEN_LIBDIR)
	mkdir -p $(PREFIX)/lib/pkgconfig
	cp solo5-bindings-muen.pc $(PREFIX)/lib/pkgconfig

.PHONY: opam-muen-uninstall
opam-muen-uninstall:
	rm -rf $(OPAM_MUEN_INCDIR) $(OPAM_MUEN_LIBDIR)
	rm -f $(PREFIX)/lib/pkgconfig/solo5-bindings-muen.pc
