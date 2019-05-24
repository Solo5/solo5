#!/bin/sh
# Copyright (c) 2015-2019 Contributors as noted in the AUTHORS file
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

prog_NAME="$(basename $0)"

die()
{
    echo "${prog_NAME}: ERROR: $@" 1>&2
    exit 1
}

warn()
{
    echo "${prog_NAME}: WARNING: $@" 1>&2
}

cc_maybe_gcc()
{
    ${CC} -dM -E - </dev/null | grep -Eq '^#define __GNUC__ [4-9]$'
}

cc_is_clang()
{
    ${CC} -dM -E - </dev/null | grep -Eq '^#define __clang__ 1$'
}

cc_has_pie()
{
    ${CC} -dM -E - </dev/null | grep -Eq '^#define __PIE__ [1-9]$'
}

cc_is_gcc()
{
    cc_maybe_gcc && ! cc_is_clang
}

gcc_check_option()
{
    ${CC} "$@" -x c -c -o /dev/null - <<EOM >/dev/null 2>&1
int main(int argc, char *argv[])
{
    return 0;
}
EOM
}

gcc_check_lib()
{
    ${CC} -x c -o /dev/null - "$@" <<EOM >/dev/null 2>&1
int main(int argc, char *argv[])
{
    return 0;
}
EOM
}

ld_is_lld()
{
    ${LD} --version 2>&1 | grep -q '^LLD'
}

# Allow external override of CC.
CC=${CC:-cc}
LD=${LD:-ld}

CC_MACHINE=$(${CC} -dumpmachine)
[ $? -ne 0 ] &&
    die "Could not run '${CC} -dumpmachine', is your compiler working?"
# Determine HOST and ARCH based on what the toolchain reports.
case ${CC_MACHINE} in
    x86_64-*linux*)
        CONFIG_ARCH=x86_64 CONFIG_HOST=Linux
        ;;
    aarch64-*linux*)
        CONFIG_ARCH=aarch64 CONFIG_HOST=Linux
        ;;
    x86_64-*freebsd*)
        CONFIG_ARCH=x86_64 CONFIG_HOST=FreeBSD
        ;;
    amd64-*openbsd*)
        CONFIG_ARCH=x86_64 CONFIG_HOST=OpenBSD
        ;;
    powerpc64le-*linux*)
        CONFIG_ARCH=ppc64le CONFIG_HOST=Linux
        ;;
    *)
        die "Unsupported toolchain target: ${CC_MACHINE}"
        ;;
esac

# Host-provided header files are installed here for in-tree builds. OPAM will
# install these to $(OPAM_INCDIR)/host where they will be picked up by
# pkg-config.
HOST_INCDIR=${PWD}/include/crt

CONFIG_HVT=
CONFIG_SPT=
CONFIG_VIRTIO=
CONFIG_MUEN=
CONFIG_GENODE=
MAKECONF_CFLAGS=
MAKECONF_LDFLAGS=
CONFIG_SPT_NO_PIE=

case "${CONFIG_HOST}" in
    Linux)
        # On Linux/gcc we use -nostdinc and copy all the gcc-provided headers.
        cc_is_gcc || die "Only 'gcc' 4.x+ is supported on Linux"
        CC_INCDIR=$(${CC} -print-file-name=include)
        [ -d "${CC_INCDIR}" ] || die "Cannot determine gcc include directory"
        mkdir -p ${HOST_INCDIR}
        cp -R ${CC_INCDIR}/. ${HOST_INCDIR}

        MAKECONF_CFLAGS="-nostdinc"
        # Recent distributions now default to PIE enabled. Disable it explicitly
        # if that's the case here.
        # XXX: This breaks MirageOS in (at least) the build of mirage-solo5 due
        # to -fno-pie breaking the build of lib/dllmirage-solo5_bindings.so.
        # Keep this disabled until that is resolved.
        # cc_has_pie && MAKECONF_CFLAGS="${MAKECONF_CFLAGS} -fno-pie"

        # Stack smashing protection:
        #
        # Any GCC configured for a Linux/x86_64 target (actually, any
        # glibc-based target) will use a TLS slot to address __stack_chk_guard.
        # Disable this behaviour and use an ordinary global variable instead.
        if [ "${CONFIG_ARCH}" = "x86_64" ]; then
            gcc_check_option -mstack-protector-guard=global || \
                die "GCC 4.9.0 or newer is required for -mstack-protector-guard= support"
            MAKECONF_CFLAGS="${MAKECONF_CFLAGS} -mstack-protector-guard=global"
        fi

        # If the host toolchain is NOT configured to build PIE exectuables by
        # default, assume it has no support for that and apply a workaround by
        # locating the spt tender starting at a virtual address of 1 GB.
        if ! cc_has_pie; then
            warn "Host toolchain does not build PIE executables, spt guest size will be limited to 1GB"
            warn "Consider upgrading to a Linux distribution with PIE support"
            CONFIG_SPT_NO_PIE=1
        fi

        CONFIG_HVT=1
        if gcc_check_lib -lseccomp; then
            CONFIG_SPT=1
        else
            warn "Could not link with -lseccomp, not building spt"
        fi
        [ "${CONFIG_ARCH}" = "x86_64" ] && CONFIG_VIRTIO=1
        [ "${CONFIG_ARCH}" = "x86_64" ] && CONFIG_MUEN=1
        [ "${CONFIG_ARCH}" = "x86_64" ] && CONFIG_GENODE=1
        [ "${CONFIG_ARCH}" = "ppc64le" ] && CONFIG_HVT=
        ;;
    FreeBSD)
        # On FreeBSD/clang we use -nostdlibinc which gives us access to the
        # clang-provided headers for compiler instrinsics. We copy the rest
        # (std*.h, float.h and their dependencies) from the host.
        cc_is_clang || die "Only 'clang' is supported on FreeBSD"
        [ "${CONFIG_ARCH}" = "x86_64" ] ||
            die "Only 'x86_64' is supported on FreeBSD"
        INCDIR=/usr/include
        SRCS_MACH="machine/_stdint.h machine/_types.h machine/endian.h \
            machine/_limits.h"
        SRCS_SYS="sys/_null.h sys/_stdint.h sys/_types.h sys/cdefs.h \
            sys/endian.h sys/_stdarg.h"
        SRCS_X86="x86/float.h x86/_stdint.h x86/stdarg.h x86/endian.h \
            x86/_types.h x86/_limits.h"
        SRCS="float.h osreldate.h stddef.h stdint.h stdbool.h stdarg.h"

        mkdir -p ${HOST_INCDIR}
        mkdir -p ${HOST_INCDIR}/machine ${HOST_INCDIR}/sys ${HOST_INCDIR}/x86
        for f in ${SRCS_MACH}; do cp -f ${INCDIR}/$f ${HOST_INCDIR}/machine; done
        for f in ${SRCS_SYS}; do cp -f ${INCDIR}/$f ${HOST_INCDIR}/sys; done
        for f in ${SRCS_X86}; do cp -f ${INCDIR}/$f ${HOST_INCDIR}/x86; done
        for f in ${SRCS}; do cp -f ${INCDIR}/$f ${HOST_INCDIR}; done

        # Stack smashing protection:
        #
        # FreeBSD toolchains use a global (non-TLS) __stack_chk_guard by
        # default on x86_64, so there is nothing special we need to do here.
        MAKECONF_CFLAGS="-nostdlibinc"

        CONFIG_HVT=1
        CONFIG_SPT=
        [ "${CONFIG_ARCH}" = "x86_64" ] && CONFIG_VIRTIO=1
        [ "${CONFIG_ARCH}" = "x86_64" ] && CONFIG_MUEN=1
        CONFIG_GENODE=
        ;;
    OpenBSD)
        # On OpenBSD/clang we use -nostdlibinc which gives us access to the
        # clang-provided headers for compiler instrinsics. We copy the rest
        # (std*.h, cdefs.h and their dependencies) from the host.
        cc_is_clang || die "Only 'clang' is supported on OpenBSD"
        [ "${CONFIG_ARCH}" = "x86_64" ] ||
            die "Only 'x86_64' is supported on OpenBSD"
        if ! ld_is_lld; then
            LD='/usr/bin/ld.lld'
            warn "Using GNU 'ld' is not supported on OpenBSD"
            warn "Falling back to 'ld.lld'"
            [ -e ${LD} ] || die "/usr/bin/ld.lld does not exist"
        fi
        INCDIR=/usr/include
        SRCS_MACH="machine/_float.h machine/endian.h machine/cdefs.h machine/_types.h"
        SRCS_SYS="sys/_null.h sys/cdefs.h sys/_endian.h sys/endian.h sys/_types.h"
        SRCS_AMD64="amd64/_float.h amd64/stdarg.h amd64/endian.h"
        SRCS="float.h stddef.h stdint.h stdbool.h stdarg.h"

        mkdir -p ${HOST_INCDIR}
        mkdir -p ${HOST_INCDIR}/machine ${HOST_INCDIR}/sys ${HOST_INCDIR}/amd64
        for f in ${SRCS_MACH}; do cp -f ${INCDIR}/$f ${HOST_INCDIR}/machine; done
        for f in ${SRCS_SYS}; do cp -f ${INCDIR}/$f ${HOST_INCDIR}/sys; done
        for f in ${SRCS_AMD64}; do cp -f ${INCDIR}/$f ${HOST_INCDIR}/amd64; done
        for f in ${SRCS}; do cp -f ${INCDIR}/$f ${HOST_INCDIR}; done

        MAKECONF_CFLAGS="-mno-retpoline -fno-ret-protector -nostdlibinc"
        MAKECONF_LDFLAGS="-nopie"

        CONFIG_HVT=1
        CONFIG_SPT=
        [ "${CONFIG_ARCH}" = "x86_64" ] && CONFIG_VIRTIO=1
        [ "${CONFIG_ARCH}" = "x86_64" ] && CONFIG_MUEN=1
        CONFIG_GENODE=
        ;;
    *)
        die "Unsupported build OS: ${CONFIG_HOST}"
        ;;
esac

# WARNING:
#
# The generated Makeconf is dual-use! It is both sourced by GNU make, and by
# the test suite. As such, a subset of this file must parse in both shell *and*
# GNU make. Given the differences in quoting rules between the two
# (unable to sensibly use VAR="VALUE"), our convention is as follows:
#
# 1. GNU make parses the entire file, i.e. all variables defined below are 
#    available to Makefiles.
#
# 2. Shell scripts parse the subset of *lines* starting with "CONFIG_". I.e.
#    only variables named "CONFIG_..." are available. When adding new variables
#    to this group you must ensure that they do not contain more than a single
#    "word".
#
# Please do NOT add variable names with new prefixes without asking first.
#
cat <<EOM >Makeconf
# Generated by configure.sh, using CC=${CC} for target ${CC_MACHINE}
CONFIG_HVT=${CONFIG_HVT}
CONFIG_SPT=${CONFIG_SPT}
CONFIG_VIRTIO=${CONFIG_VIRTIO}
CONFIG_MUEN=${CONFIG_MUEN}
CONFIG_GENODE=${CONFIG_GENODE}
MAKECONF_CFLAGS=${MAKECONF_CFLAGS}
MAKECONF_LDFLAGS=${MAKECONF_LDFLAGS}
CONFIG_ARCH=${CONFIG_ARCH}
CONFIG_HOST=${CONFIG_HOST}
MAKECONF_CC=${CC}
MAKECONF_LD=${LD}
CONFIG_SPT_NO_PIE=${CONFIG_SPT_NO_PIE}
EOM

echo "${prog_NAME}: Configured for ${CC_MACHINE}."
echo -n "${prog_NAME}: Enabled targets:"
[ -n "${CONFIG_HVT}" ]    && echo -n " hvt"
[ -n "${CONFIG_SPT}" ]    && echo -n " spt"
[ -n "${CONFIG_VIRTIO}" ] && echo -n " virtio"
[ -n "${CONFIG_MUEN}" ]   && echo -n " muen"
[ -n "${CONFIG_GENODE}" ] && echo -n " genode"
echo "."
