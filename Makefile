# Copyright (c) 2015, IBM
# Author(s): Dan Williams <djwillia@us.ibm.com>
#
# Permission to use, copy, modify, and/or distribute this software for
# any purpose with or without fee is hereby granted, provided that the
# above copyright notice and this permission notice appear in all
# copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
# DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
# OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

all: kernel ukvm

.PHONY: kernel
kernel: 
	make -C kernel

.PHONY: ukvm
ukvm:
	make -C ukvm

run: ukvm kernel disk.img
	sudo time -f"%E elapsed" ukvm/ukvm kernel/test_hello.bin disk.img tap100

# nothing needs to be on the disk image, it just needs to exist
disk.img:
	dd if=/dev/zero of=disk.img bs=1M count=1

.PHONY: clean
clean:
	make -C kernel clean
	make -C ukvm clean
	$(RM) solo5-kernel-ukvm.pc

PREFIX?=/nonexistent # Fail if not run from OPAM
OPAM_INCDIR=$(PREFIX)/include/solo5-kernel-ukvm/include
OPAM_LIBDIR=$(PREFIX)/lib/solo5-kernel-ukvm
OPAM_BINDIR=$(PREFIX)/bin

# We want the MD CFLAGS in the .pc file, where they can be (eventually) picked
# up by the Mirage tool. XXX We may want to pick LDLIBS and LDFLAGS also.
KERNEL_MD_CFLAGS=$(shell make -sC kernel print-md-cflags)
%.pc: %.pc.in 
	sed <$< > $@ \
	    -e 's#!CFLAGS!#$(KERNEL_MD_CFLAGS)#g;'

.PHONY: opam-install
# TODO: solo5.h and ukvm.h should only contain public APIs.
opam-install: solo5-kernel-ukvm.pc
	mkdir -p $(OPAM_INCDIR) $(OPAM_LIBDIR)
	cp kernel/kernel.h $(OPAM_INCDIR)/solo5.h
	cp ukvm/ukvm.h $(OPAM_INCDIR)/ukvm.h
	cp kernel/solo5.o kernel/solo5.lds $(OPAM_LIBDIR)
	cp ukvm/ukvm $(OPAM_BINDIR)
	cp solo5-kernel-ukvm.pc $(PREFIX)/lib/pkgconfig

.PHONY: opam-uninstall
opam-uninstall:
	rm -rf $(OPAM_INCDIR) $(OPAM_LIBDIR)
	rm -f $(OPAM_BINDIR)/ukvm
	rm -f $(PREFIX)/lib/pkgconfig/solo5-kernel-ukvm.pc
