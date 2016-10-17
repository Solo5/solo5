#!/bin/sh

die()
{
    echo "$0: $@" 1>&2
    exit 1
}

cc_is_clang()
{
    ${CC:-cc} -v 2>&1 | grep -q "clang version"
}

cc_is_gcc()
{
    ${CC:-cc} -v 2>&1 | grep -q "^gcc version"
}

check_ld_version()
{
    type $1 >/dev/null 2>&1 && $1 -v | grep -q 'GNU.*2.2[5-9]'
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
        # (std*.h, float.h) from the host.
        cc_is_clang || die "Only 'clang' is supported on FreeBSD"
        INCDIR=/usr/include
        SRCS_MACH="machine/_stdint.h machine/_types.h machine/endian.h \
            machine/_limits.h"
        SRCS_SYS="sys/_null.h sys/_stdint.h sys/_types.h sys/cdefs.h"
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
        # Check that the linker is new enough. Allow the user to explicitly
        # override ${LD}; if unset default to looking for the binutils one.
        LD=${LD:-/usr/local/bin/ld}
        check_ld_version ${LD} || \
            die "Linker '${LD}': Not found or too old (< 2.25)." \
            "Please install ld from 'binutils' or set \$LD explicitly."
        EXTRA="LD=${LD}"
        ;;
    *)
        die "Unsupported build OS: $(uname -s)"
        ;;
esac

cat <<EOM >Makeconf
BUILD_UKVM=${BUILD_UKVM}
BUILD_VIRTIO=${BUILD_VIRTIO}
HOST_CFLAGS=${HOST_CFLAGS}
${EXTRA}
EOM
