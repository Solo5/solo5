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

die()
{
    echo "$0: $@" 1>&2
    exit 1
}

cc_is_clang()
{
    ${CC:-cc} -v 2>&1 | grep -q "clang"
}

cc_is_gcc()
{
    ${CC:-cc} -v 2>&1 | grep -q "^gcc version"
}

ld_is_x86_64_elf()
{
    ${LD:-ld} --help 2>&1 |grep "supported targets" 2>&1 | grep -q elf64-x86-64
}

objcopy_exists()
{
    ${OBJCOPY:-objcopy} --version 2>&1 | grep -q "GNU objcopy"
}

# Host-provided header files are installed here for in-tree builds. OPAM will
# install these to $(OPAM_INCDIR)/host where they will be picked up by
# pkg-config.
HOST_INCDIR=${PWD}/include-host

case $(uname -s) in
    Linux)
        # On Linux/gcc we use -nostdinc and copy all the gcc-provided headers.
        cc_is_gcc || die "Only 'gcc' is supported on Linux"
        CC_INCDIR=$(${CC:-cc} -print-file-name=include)
        [ -d "${CC_INCDIR}" ] || die "Cannot determine gcc include directory"
        mkdir -p ${HOST_INCDIR}
        cp -R ${CC_INCDIR}/. ${HOST_INCDIR}

        HOST_CFLAGS="-nostdinc"
        BUILD_UKVM="yes"
        BUILD_VIRTIO="yes"
        ;;
    FreeBSD)
        # On FreeBSD/clang we use -nostdlibinc which gives us access to the
        # clang-provided headers for compiler instrinsics. We copy the rest
        # (std*.h, float.h and their dependencies) from the host.
        cc_is_clang || die "Only 'clang' is supported on FreeBSD"
        INCDIR=/usr/include
        SRCS_MACH="machine/_stdint.h machine/_types.h machine/endian.h \
            machine/_limits.h"
        SRCS_SYS="sys/_null.h sys/_stdint.h sys/_types.h sys/cdefs.h \
            sys/endian.h"
        SRCS_X86="x86/float.h x86/_stdint.h x86/stdarg.h x86/endian.h \
            x86/_types.h x86/_limits.h"
        SRCS="float.h osreldate.h stddef.h stdint.h stdbool.h stdarg.h"

        mkdir -p ${HOST_INCDIR}
        mkdir -p ${HOST_INCDIR}/machine ${HOST_INCDIR}/sys ${HOST_INCDIR}/x86
        for f in ${SRCS_MACH}; do cp -f ${INCDIR}/$f ${HOST_INCDIR}/machine; done
        for f in ${SRCS_SYS}; do cp -f ${INCDIR}/$f ${HOST_INCDIR}/sys; done
        for f in ${SRCS_X86}; do cp -f ${INCDIR}/$f ${HOST_INCDIR}/x86; done
        for f in ${SRCS}; do cp -f ${INCDIR}/$f ${HOST_INCDIR}; done

        HOST_CFLAGS="-nostdlibinc"
        BUILD_UKVM=
        BUILD_VIRTIO="yes"
        ;;
    Darwin)
        # On MacOSX we use clang and approximate the FreeBSD header
        # copying.  While clang is good enough to build the kernel, we
        # need a cross-linker that understands ELF, rather than
        # Mach-O.  As of Jan 2016, the llvm linker was not able to
        # link the unikernel, so we require a GNU binutils linker (and
        # objcopy).
        #
        # Binutils from Homebrew does not contain ld, so you can build
        # from source.  After downloading binutils source, do:
        #
        #   ./configure --prefix=/usr/local/opt/binutils-x86_64 \
        #       --target=x86_64-elf --disable-nls --disable-werror
        #    make
        #    make install
        #
        # Then add the newly compiled cross-binutils to your path
        #    export PATH="/usr/local/opt/binutils-x86_64/bin:$PATH"
        cc_is_clang || die "Only 'clang' is supported on MacOSX"
        # ld_is_x86_64_elf || die "LD must be a cross-linker for elf64-x86-64." \
        #                         "You may need to build GNU binutils from source."
        objcopy_exists || die "OBJCOPY should point to GNU objcopy." \
                              "You may need to build GNU binutils from source."
        INCDIR=/usr/include
        SRCS_MACH="machine/_types.h machine/endian.h \
            machine/_limits.h"
        SRCS_SYS="sys/cdefs.h"
        SRCS_X86="i386/endian.h i386/_types.h i386/_limits.h"
        SRCS="_types.h"

        mkdir -p ${HOST_INCDIR}
        mkdir -p ${HOST_INCDIR}/machine ${HOST_INCDIR}/sys ${HOST_INCDIR}/i386
        for f in ${SRCS_MACH}; do cp -f ${INCDIR}/$f ${HOST_INCDIR}/machine; done
        for f in ${SRCS_SYS}; do cp -f ${INCDIR}/$f ${HOST_INCDIR}/sys; done
        for f in ${SRCS_X86}; do cp -f ${INCDIR}/$f ${HOST_INCDIR}/i386; done
        for f in ${SRCS}; do cp -f ${INCDIR}/$f ${HOST_INCDIR}; done

        #HOST_CFLAGS="-nostdlibinc -target x86_64-elf"
        HOST_CFLAGS="-nostdlibinc"
        BUILD_UKVM="yes"
        BUILD_VIRTIO=
        ;;
    *)
        die "Unsupported build OS: $(uname -s)"
        ;;
esac

cat <<EOM >Makeconf
BUILD_UKVM=${BUILD_UKVM}
BUILD_VIRTIO=${BUILD_VIRTIO}
HOST_CFLAGS=${HOST_CFLAGS}
EOM
