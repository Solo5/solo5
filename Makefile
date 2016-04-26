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

all: loader kernel

.PHONY: loader
loader:
	make -C loader

.PHONY: kernel
kernel: 
	make -C kernel

kernel.iso: loader kernel iso/boot/grub/menu.lst Makefile
	@cp loader/loader iso/boot/
	@cp kernel/kernel iso/boot/
	@xorriso -as mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot \
		-boot-load-size 4 -quiet -boot-info-table -o kernel.iso iso

test.iso: loader kernel iso/boot/grub/menu.lst Makefile
	@cp loader/loader iso/boot/
	@cp kernel/test_hello.bin iso/boot/kernel
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
qemu: kernel.iso disk.img
	sudo qemu-system-x86_64 -s -nographic -name foo -m 1024 -cdrom kernel.iso -net nic,model=virtio -net tap,ifname=veth0,script=kvm-br.bash -drive file=disk.img,if=virtio -boot d

kvm: kernel.iso disk.img
	sudo kvm -s -nographic -name foo -m 1024 -cdrom kernel.iso -net nic,model=virtio -net tap,ifname=veth0,script=kvm-br.bash -drive file=disk.img,if=virtio -boot d

xen: kernel.iso
	xl create -c kernel.cfg

clean:
	@echo -n cleaning...
	@make -C loader clean
	@make -C kernel clean
	@rm -f kernel.iso iso/boot/kernel iso/boot/loader
	@rm -f solo5-kernel-virtio.pc
	@echo done


PREFIX?=/nonexistent # Fail if not run from OPAM
OPAM_INCDIR=$(PREFIX)/include/solo5-kernel-virtio/include
OPAM_LIBDIR=$(PREFIX)/lib/solo5-kernel-virtio
OPAM_BINDIR=$(PREFIX)/bin

# We want the MD CFLAGS in the .pc file, where they can be (eventually) picked
# up by the Mirage tool. XXX We may want to pick LDLIBS and LDFLAGS also.
KERNEL_MD_CFLAGS=$(shell make -sC kernel print-md-cflags)
%.pc: %.pc.in 
	sed <$< > $@ \
	    -e 's#!CFLAGS!#$(KERNEL_MD_CFLAGS)#g;'

.PHONY: opam-install
# TODO: solo5.h and ukvm.h should only contain public APIs.
opam-install: solo5-kernel-virtio.pc kernel loader
	mkdir -p $(OPAM_INCDIR) $(OPAM_LIBDIR)
	cp kernel/kernel.h $(OPAM_INCDIR)/solo5.h
	cp loader/loader $(OPAM_LIBDIR)
	cp iso/boot/grub/menu.lst $(OPAM_LIBDIR)
	cp iso/boot/grub/stage2_eltorito $(OPAM_LIBDIR)
	cp kernel/solo5.o kernel/solo5.lds $(OPAM_LIBDIR)
	cp solo5-kernel-virtio.pc $(PREFIX)/lib/pkgconfig

.PHONY: opam-uninstall
opam-uninstall:
	rm -rf $(OPAM_INCDIR) $(OPAM_LIBDIR)
	rm -f $(PREFIX)/lib/pkgconfig/solo5-kernel-virtio.pc
