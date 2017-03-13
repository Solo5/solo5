#!/bin/sh
# Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
#
# This file is part of Solo5, a unikernel base layer.
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

#
# solo5-mkimage: Builds an MBR-partitioned disk image with SYSLINUX and a
# unikernel in it. The image file is sparse, sized to 1GB (or modify $SIZE
# below).
#
# Requirements: A Linux system with sfdisk, mtools and syslinux installed.
# Path names for syslinux library files are based on those for Debian/Alpine,
# YMMV.
#
usage ()
{
    cat <<EOM 1>&2
usage: solo5-mkimage [ OPTIONS ] [ -- ] IMAGE UNIKERNEL [ ARGUMENTS ... ]

Builds an MBR-partitioned disk IMAGE with a boot loader and UNIKERNEL in it.
ARGUMENTS, if specified, will be embedded in the image and passed to the
unikernel at boot time.

UNIKERNEL must be a unikernel built for the 'virtio' target.

Options:
    -d: Run in a Docker container (using mato/solo5-mkimage).

    -f FORMAT: Output a 'raw' disk image (default) or a 'tar' file suitable
        for uploading to Google Compute Engine.
EOM
    exit 1
}

die ()
{
    echo solo5-mkimage: error: "$@" 1>&2
    if [ -n "${TMPDIR}" -a -f "${TMPDIR}"/log ]; then
        echo For more information, see ${TMPDIR}/log
        PRESERVE_TMPDIR=1
    fi
    exit 1
}

nuketmpdir ()
{
    [ -n "${PRESERVE_TMPDIR}" ] && return
    [ -z "${TMPDIR}" ] && return
    [ ! -d "${TMPDIR}" ] && return
    rm -rf ${TMPDIR}
}

maybefind ()
{
    for f in "$@"; do
        [ -f $f ] && echo $f && return 0
    done
    return 1
}

# Parse command line arguments.
ARGS=$(getopt df: $*)
[ $? -ne 0 ] && usage
set -- $ARGS
RUN_IN_DOCKER=
FORMAT=raw
while true; do
    case "$1" in
    -d)
        RUN_IN_DOCKER=yes
        shift
        ;;
    -f)
        case "$2" in
        tar|raw)
            FORMAT="$2"
            shift; shift
            ;;
        *)
            usage
            ;;
        esac
        ;;
    --)
        shift; break
        ;;
    esac
done
[ $# -lt 2 ] && usage

IMAGE=$(readlink -f $1)
shift
UNIKERNEL=$(readlink -f $1)
shift
[ ! -f ${UNIKERNEL} ] && die "not found: ${UNIKERNEL}"
SIZE=1G

if [ "$(uname -s)" != "Linux" -a -z "${RUN_IN_DOCKER}" ]; then
    die "Sorry, this tool only runs on Linux, or in Docker with the -d option."
fi

if [ -n "${RUN_IN_DOCKER}" ]; then
    # User wants to run us in a container. Figure out the destination and source
    # directories (containing IMAGE and UNIKERNEL) to bind-mount from the host.
    DESTDIR=$(dirname ${IMAGE}) 2>/dev/null
    [ ! -d ${DESTDIR} ] && die "can't get parent directory of: ${IMAGE}"
    SRCDIR=$(dirname ${UNIKERNEL})
    [ ! -d ${SRCDIR} ] && die "can't get parent directory of: ${UNIKERNEL}"
    DESTFILE=$(basename ${IMAGE})
    SRCFILE=$(basename ${UNIKERNEL})

    # --tmpfs is only supported from 1.10.0 onwards
    if docker run --help | grep -q tmpfs; then
        TMPFS="--tmpfs /tmp:rw,noexec,nosuid,size=2g"
    else
        TMPFS=""
    fi

    exec docker run --rm \
        ${TMPFS} \
        -v ${SRCDIR}:/host/src -v ${DESTDIR}:/host/dest \
        mato/solo5-mkimage -f ${FORMAT} \
            -- \
            /host/dest/${DESTFILE} /host/src/${SRCFILE} "$@"
fi

# Check dependencies.
type sfdisk >/dev/null 2>/dev/null || die "need sfdisk installed"
type syslinux >/dev/null 2>/dev/null || die "need syslinux installed"
type mtools >/dev/null 2>/dev/null || die "need mtools installed"
type mkdosfs >/dev/null 2>/dev/null || die "need dosfstools installed"

SYSLINUX_MBR=$(maybefind /usr/lib/syslinux/mbr/mbr.bin \
                         /usr/share/syslinux/mbr.bin \
                         /usr/lib/syslinux/bios/mbr.bin) || die "can't find syslinux mbr.bin"
SYSLINUX_COM32=$(maybefind /usr/lib/syslinux/modules/bios/libcom32.c32 \
                           /usr/share/syslinux/libcom32.c32 \
                           /usr/lib/syslinux/bios/libcom32.c32) || die "can't find syslinux libcom32.c32"
SYSLINUX_MBOOT=$(maybefind /usr/lib/syslinux/modules/bios/mboot.c32 \
                           /usr/share/syslinux/mboot.c32 \
                           /usr/lib/syslinux/bios/mboot.c32) || die "can't find syslinux mboot.c32"

trap nuketmpdir 0 INT TERM
TMPDIR=$(mktemp -d)
if [ $? -ne 0 ]; then
    echo "error creating temporary directory" 1>&2
    exit 1
fi

LOG=${TMPDIR}/log
# Write SYSLINUX MBR to image and extend to desired size (sparse)
cp ${SYSLINUX_MBR} ${TMPDIR}/image.mbr || die "could not copy mbr"
truncate -s ${SIZE} ${TMPDIR}/image.mbr
# Create DOS (FAT32) primary partition
# sfdisk no longer supports -D
echo ",,0xc,*" | sfdisk ${TMPDIR}/image.mbr >${LOG} 2>&1 || die "sfdisk failed"
# Start offset of partition (sectors)
O_SECTORS=$(sfdisk -d ${TMPDIR}/image.mbr | awk -- '/(Id= |type=)c/{print $4}' \
    | sed -e s/,//)
[ -n "${O_SECTORS}" ] || die "internal error, can't parse sfdisk"
# Size of partition (sectors)
S_SECTORS=$(sfdisk -d ${TMPDIR}/image.mbr | awk -- '/(Id= |type=)c/{print $6}' \
    | sed -e s/,//)
[ -n "${S_SECTORS}" ] || die "internal error, can't parse sfdisk"
# Start offset of partition (bytes)
O_BYTES=$(expr ${O_SECTORS} \* 512)
# Size of partition (1k blocks, which mkdosfs expects)
S_BLOCKS=$(expr ${S_SECTORS} / 2)
# Extract partition from image
dd if=${TMPDIR}/image.mbr of=${TMPDIR}/image.dos bs=512 skip=${O_SECTORS} \
    conv=sparse 2>/dev/null || die "dd failed"
# Truncate image.mbr to contain only the MBR + padding before partition start
truncate -s ${O_BYTES} ${TMPDIR}/image.mbr

# Create FAT32 filesystem, install SYSLINUX
mkdosfs -F 32 ${TMPDIR}/image.dos ${S_BLOCKS} >${LOG} 2>&1 \
    || die "mkfs.msdos failed"
syslinux --install ${TMPDIR}/image.dos
cat <<EOM >${TMPDIR}/syslinux.cfg
SERIAL 0 115200
DEFAULT unikernel
LABEL unikernel
  KERNEL mboot.c32
  APPEND unikernel.bin $@
EOM
# Ugh, mtools complains about filesystem size not being a multiple of
# what it thinks the sectors-per-track are, ignore.
echo mtools_skip_check=1 > ${TMPDIR}/mtoolsrc
export MTOOLSRC=${TMPDIR}/mtoolsrc
# Populate filesystem
mcopy -i ${TMPDIR}/image.dos ${TMPDIR}/syslinux.cfg ::syslinux.cfg \
    || die "copy failed"
mcopy -i ${TMPDIR}/image.dos ${SYSLINUX_COM32} ::libcom32.c32 \
    || die "copy failed"
mcopy -i ${TMPDIR}/image.dos ${SYSLINUX_MBOOT} ::mboot.c32 \
    || die "copy failed"
mcopy -i ${TMPDIR}/image.dos ${UNIKERNEL} ::unikernel.bin \
    || die "copy failed"

# Construct final image (MBR + padding + filesystem)
if [ "${FORMAT}" = "raw" ]; then
    cat ${TMPDIR}/image.mbr ${TMPDIR}/image.dos \
        | dd of=${IMAGE} conv=sparse 2>/dev/null || die "dd failed"
else
    cat ${TMPDIR}/image.mbr ${TMPDIR}/image.dos \
        | dd of=${TMPDIR}/disk.raw conv=sparse 2>/dev/null || die "dd failed"
    (cd ${TMPDIR} && tar -Sczf ${IMAGE} disk.raw) || die "tar failed"
fi

nuketmpdir
