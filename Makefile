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
	make -C loader
	make -C kernel virtio

.PHONY: ukvm_target
ukvm_target:
	make -C ukvm
	make -C kernel ukvm

test.iso: virtio_target iso/boot/grub/menu.lst Makefile
	@cp loader/loader iso/boot/
	@cp kernel/test_ping_serve.virtio iso/boot/kernel
	@xorriso -as mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot \
		-boot-load-size 4 -quiet -boot-info-table -o test.iso iso

# nothing needs to be on the disk image, it just needs to exist
disk.img:
	dd if=/dev/zero of=disk.img bs=1M count=1

# The option:
#     -net dump,file=net.pcap
# dumps the network output to a file.  It can be read with:
#     tcpdump -nr net.pcap
# Use the option 
#     -nographic 
# to have serial redirected to console like Xen HVM, and use C-a x to
# exit QEMU afterwards
qemu: test.iso disk.img
	sudo qemu-system-x86_64 -s -nographic -name foo -m 1024 -cdrom test.iso -net nic,model=virtio -net tap,ifname=veth0,script=kvm-br.bash -drive file=disk.img,if=virtio -boot d

kvm: test.iso disk.img
	sudo kvm -s -nographic -name foo -m 1024 -boot d -cdrom test.iso \
		 -device virtio-net,netdev=n0 \
		 -netdev tap,id=n0,ifname=tap100,script=no,downscript=no \
		 -drive file=disk.img,if=virtio 

ukvm: ukvm_target disk.img
	sudo ukvm/ukvm kernel/test_hello.ukvm disk.img tap100

xen: test.iso
	xl create -c kernel.cfg

clean:
	@echo -n cleaning...
	@make -C loader clean
	@make -C kernel clean
	@make -C ukvm clean
	@rm -f test.iso iso/boot/kernel iso/boot/loader
	@rm -f solo5-kernel-virtio.pc
	@rm -f solo5-kernel-ukvm.pc
	@echo done


PREFIX?=/nonexistent # Fail if not run from OPAM
OPAM_BINDIR=$(PREFIX)/bin
OPAM_UKVM_LIBDIR=$(PREFIX)/lib/solo5-kernel-ukvm
OPAM_UKVM_INCDIR=$(PREFIX)/include/solo5-kernel-ukvm/include
OPAM_VIRTIO_LIBDIR=$(PREFIX)/lib/solo5-kernel-virtio
OPAM_VIRTIO_INCDIR=$(PREFIX)/include/solo5-kernel-virtio/include

# We want the MD CFLAGS in the .pc file, where they can be
# (eventually) picked up by the Mirage tool. XXX We may want to pick
# LDLIBS and LDFLAGS also.
KERNEL_MD_CFLAGS=$(shell make -sC kernel print-md-cflags)
%.pc: %.pc.in 
	sed <$< > $@ \
	    -e 's#!CFLAGS!#$(KERNEL_MD_CFLAGS)#g;'

.PHONY: opam-virtio-install
# TODO: solo5.h and ukvm.h should only contain public APIs.
opam-virtio-install: solo5-kernel-virtio.pc virtio_target
	mkdir -p $(OPAM_VIRTIO_INCDIR) $(OPAM_VIRTIO_LIBDIR)
	cp kernel/kernel.h $(OPAM_VIRTIO_INCDIR)/solo5.h
	cp loader/loader $(OPAM_VIRTIO_LIBDIR)
	cp iso/boot/grub/menu.lst $(OPAM_VIRTIO_LIBDIR)
	cp iso/boot/grub/stage2_eltorito $(OPAM_VIRTIO_LIBDIR)
	cp kernel/virtio/solo5.o kernel/virtio/solo5.lds $(OPAM_VIRTIO_LIBDIR)
	cp solo5-kernel-virtio.pc $(PREFIX)/lib/pkgconfig

.PHONY: opam-virtio-uninstall
opam-virtio-uninstall:
	rm -rf $(OPAM_VIRTIO_INCDIR) $(OPAM_VIRTIO_LIBDIR)
	rm -f $(PREFIX)/lib/pkgconfig/solo5-kernel-virtio.pc

.PHONY: opam-ukvm-install
# TODO: solo5.h and ukvm.h should only contain public APIs.
opam-ukvm-install: solo5-kernel-ukvm.pc ukvm_target
	mkdir -p $(OPAM_UKVM_INCDIR) $(OPAM_UKVM_LIBDIR)
	cp kernel/kernel.h $(OPAM_UKVM_INCDIR)/solo5.h
	cp ukvm/ukvm.h $(OPAM_UKVM_INCDIR)/ukvm.h
	cp kernel/ukvm/solo5.o kernel/ukvm/solo5.lds $(OPAM_UKVM_LIBDIR)
	cp ukvm/ukvm $(OPAM_BINDIR)
	cp solo5-kernel-ukvm.pc $(PREFIX)/lib/pkgconfig

.PHONY: opam-ukvm-uninstall
opam-ukvm-uninstall:
	rm -rf $(OPAM_UKVM_INCDIR) $(OPAM_UKVM_LIBDIR)
	rm -f $(OPAM_BINDIR)/ukvm
	rm -f $(PREFIX)/lib/pkgconfig/solo5-kernel-ukvm.pc
