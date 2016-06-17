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

all: ukvm_target virtio_target

.PHONY: virtio_target
virtio_target:
	make -C kernel virtio

.PHONY: ukvm_target
ukvm_target:
	make -C ukvm
	make -C kernel ukvm

test.iso: virtio_target iso/boot/grub/menu.lst Makefile
	@cp kernel/test_ping_serve.virtio iso/boot/kernel
	@xorriso -as mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot \
		-boot-load-size 4 -quiet -boot-info-table -o test.iso iso

# nothing needs to be on the disk image, it just needs to exist
disk.img:
	dd if=/dev/zero of=disk.img bs=1M count=1

qemu: test.iso disk.img
	sudo qemu-system-x86_64 -s -nographic -name foo -m 1024 -boot d -cdrom test.iso \
		 -device virtio-net,netdev=n0 \
		 -netdev tap,id=n0,ifname=tap100,script=no,downscript=no \
		 -drive file=disk.img,if=virtio 

kvm: test.iso disk.img
	sudo kvm -s -nographic -name foo -m 1024 -boot d -cdrom test.iso \
		 -device virtio-net,netdev=n0 \
		 -netdev tap,id=n0,ifname=tap100,script=no,downscript=no \
		 -drive file=disk.img,if=virtio 

ukvm: ukvm_target disk.img
	sudo ukvm/ukvm disk.img tap100 kernel/test_hello.ukvm

gdb: ukvm_target disk.img
	sudo time -f"%E elapsed" ukvm/ukvm kernel/test_hello.ukvm disk.img tap100 --gdb

clean:
	@echo -n cleaning...
	@make -C kernel clean
	@make -C ukvm clean
	@rm -f test.iso iso/boot/kernel
	@rm -f solo5-kernel-virtio.pc
	@rm -f solo5-kernel-ukvm.pc
	@echo done


PREFIX?=/nonexistent # Fail if not run from OPAM
OPAM_BINDIR=$(PREFIX)/bin
OPAM_UKVM_LIBDIR=$(PREFIX)/lib/solo5-kernel-ukvm
OPAM_UKVM_INCDIR=$(PREFIX)/include/solo5-kernel-ukvm/include
OPAM_VIRTIO_LIBDIR=$(PREFIX)/lib/solo5-kernel-virtio
OPAM_VIRTIO_INCDIR=$(PREFIX)/include/solo5-kernel-virtio/include

# We want the MD CFLAGS, LDFLAGS and LDLIBS in the .pc file, where they can be
# picked up by the Mirage tool / other downstream consumers.
KERNEL_MD_CFLAGS=$(shell make -sC kernel print-md-cflags)
KERNEL_LDFLAGS=$(shell make -sC kernel print-ldflags)
KERNEL_LDLIBS=$(shell make -sC kernel print-ldlibs)
%.pc: %.pc.in 
	sed <$< > $@ \
	    -e 's#!CFLAGS!#$(KERNEL_MD_CFLAGS)#g;' \
	    -e 's#!LDFLAGS!#$(KERNEL_LDFLAGS)#g;' \
	    -e 's#!LDLIBS!#$(KERNEL_LDLIBS)#g;'

.PHONY: opam-virtio-install
opam-virtio-install: solo5-kernel-virtio.pc virtio_target
	mkdir -p $(OPAM_VIRTIO_INCDIR) $(OPAM_VIRTIO_LIBDIR)
	cp kernel/solo5.h $(OPAM_VIRTIO_INCDIR)/solo5.h
	cp iso/boot/grub/menu.lst $(OPAM_VIRTIO_LIBDIR)
	cp iso/boot/grub/stage2_eltorito $(OPAM_VIRTIO_LIBDIR)
	cp kernel/virtio/solo5.o kernel/virtio/solo5.lds $(OPAM_VIRTIO_LIBDIR)
	mkdir -p $(OPAM_BINDIR)
	mkdir -p $(PREFIX)/lib/pkgconfig
	cp solo5-kernel-virtio.pc $(PREFIX)/lib/pkgconfig
	cp solo5-build-iso.bash $(OPAM_BINDIR)

.PHONY: opam-virtio-uninstall
opam-virtio-uninstall:
	rm -rf $(OPAM_VIRTIO_INCDIR) $(OPAM_VIRTIO_LIBDIR)
	rm -f $(PREFIX)/lib/pkgconfig/solo5-kernel-virtio.pc
	rm -f $(OPAM_BINDIR)/solo5-build-iso.bash

.PHONY: opam-ukvm-install
opam-ukvm-install: solo5-kernel-ukvm.pc ukvm_target
	mkdir -p $(OPAM_UKVM_INCDIR) $(OPAM_UKVM_LIBDIR)
	cp kernel/solo5.h $(OPAM_UKVM_INCDIR)/solo5.h
	cp ukvm/ukvm.h $(OPAM_UKVM_INCDIR)/ukvm.h
	cp kernel/ukvm/solo5.o kernel/ukvm/solo5.lds $(OPAM_UKVM_LIBDIR)
	mkdir -p $(OPAM_BINDIR)
	cp ukvm/ukvm $(OPAM_BINDIR)
	mkdir -p $(PREFIX)/lib/pkgconfig
	cp solo5-kernel-ukvm.pc $(PREFIX)/lib/pkgconfig

.PHONY: opam-ukvm-uninstall
opam-ukvm-uninstall:
	rm -rf $(OPAM_UKVM_INCDIR) $(OPAM_UKVM_LIBDIR)
	rm -f $(OPAM_BINDIR)/ukvm
	rm -f $(PREFIX)/lib/pkgconfig/solo5-kernel-ukvm.pc
