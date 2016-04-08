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

all: kernel.iso

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
	@echo done


include mirage.config

config_console: 
	bash config_mirage.bash $(CFG_M_APP_CONSOLE_D) > mirage.mk
	make -C kernel mirage_libs.mk
config_block: 
	bash config_mirage.bash $(CFG_M_APP_BLOCK_D) > mirage.mk
	make -C kernel mirage_libs.mk
config_kv_ro_crunch: 
	bash config_mirage.bash $(CFG_M_APP_KV_RO_CRUNCH_D) > mirage.mk
	make -C kernel mirage_libs.mk
config_kv_ro: 
	bash config_mirage.bash $(CFG_M_APP_KV_RO_D) > mirage.mk
	make -C kernel mirage_libs.mk
config_stackv4:
	bash config_mirage.bash $(CFG_M_APP_STACKV4_D) > mirage.mk
	make -C kernel mirage_libs.mk
config_static_web:
	bash config_mirage.bash $(CFG_M_APP_STATIC_WEB_D) > mirage.mk
	make -C kernel mirage_libs.mk

# www on solo5/mirage seems to be broken
# config_www:
# 	bash config_mirage.bash $(CFG_M_APP_WWW_D) > mirage.mk
# 	make -C kernel mirage_libs.mk

# config_blog5:
# 	bash config_mirage.bash ../blog5/mirage > mirage.mk
# 	make -C kernel mirage_libs.mk


