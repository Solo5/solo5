
[ $# -eq 0 ] && { echo "Usage: $0 filename.virtio"; exit 1; }

FILE=$1
ISO=${FILE/.virtio/.iso}
echo "Creating $ISO from $FILE"

export PKG_CONFIG_PATH=`opam config var prefix`/lib/pkgconfig
LIBDIR=`pkg-config --variable=libdir solo5-kernel-virtio`

mkdir -p /tmp/iso/boot/grub
cp $LIBDIR/menu.lst /tmp/iso/boot/grub
cp $LIBDIR/stage2_eltorito /tmp/iso/boot/grub
cp $LIBDIR/loader /tmp/iso/boot/
cp $FILE /tmp/iso/boot/kernel

xorriso -as mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot -boot-load-size 4 -quiet -boot-info-table -o $ISO /tmp/iso
