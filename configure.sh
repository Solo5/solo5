#!/bin/sh
# Copyright (c) 2015-2020 Contributors as noted in the AUTHORS file
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

err()
{
    echo "${prog_NAME}: ERROR: $@" 1>&2
}

die()
{
    echo "${prog_NAME}: ERROR: $@" 1>&2
    exit 1
}

warn()
{
    echo "${prog_NAME}: WARNING: $@" 1>&2
}

usage()
{
    cat <<EOM 1>&2
usage: ${prog_NAME} [ OPTIONS ]

Configures the Solo5 build system.

Options:
    --prefix=DIR:
        Installation prefix (default: /usr/local).

    --enable-targets=none | TARGET,...
        Enables the specified Solo5 TARGETs.
        TARGET may be one of: '${AVAILABLE_TARGETS}'.
        (default: all of the above, depends on architecture and build OS).
EOM
    exit 1
}

cc_maybe_gcc()
{
    ${CC} -dM -E - </dev/null | grep -Eq '^#define __GNUC__ ([4-9]$|[1-9][0-9]+$)'
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

gcc_check_header()
{
    ${CC} ${PKG_CFLAGS} -x c -o /dev/null - <<EOM >/dev/null 2>&1
#include <$@>

int main(int argc, char *argv[])
{
    return 0;
}
EOM
}

gcc_check_lib()
{
    ${CC} -x c -o /dev/null - "$@" ${PKG_LIBS} <<EOM >/dev/null 2>&1
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

# Arguments: PATH, FILES...
# For the header FILES..., all of which must be relative to PATH, resolve their
# dependencies using the C preprocessor and output a list of FILES... plus all
# their unique dependencies, also relative to PATH.
get_header_deps()
{
    local path="$1"
    shift
    (
        # XXX This will leak ${temp} on failure, too bad.
        temp="$(mktemp)"
        cd ${path} || exit 1
        ${CC} -M "$@" >${temp} || exit 1
        sed -e 's!.*\.o:!!g' -e "s!${path}/!!g" ${temp} \
            | tr ' \\' '\n' \
            | sort \
            | uniq
        rm ${temp}
    )
}

config_host_linux()
{
    # On Linux/gcc we use -nostdinc and copy all the gcc-provided headers.
    cc_is_gcc || die "Only 'gcc' 4.x+ is supported on Linux"
    CC_INCDIR=$(${CC} -print-file-name=include)
    [ -d "${CC_INCDIR}" ] || die "Cannot determine gcc include directory"
    mkdir -p ${CRT_INCDIR}
    cp -Rp ${CC_INCDIR}/. ${CRT_INCDIR}

    MAKECONF_CFLAGS="${MAKECONF_CFLAGS} -nostdinc"
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
    if [ "${CONFIG_ARCH}" = "x86_64" ] || [ "${CONFIG_ARCH}" = "ppc64le" ]; then
        gcc_check_option -mstack-protector-guard=global || \
            die "GCC 4.9.0 or newer is required for -mstack-protector-guard= support"
        MAKECONF_CFLAGS="${MAKECONF_CFLAGS} -mstack-protector-guard=global"
    fi

    # The following runes are all spt-specific, so just return here if the
    # caller doesn't want CONFIG_SPT.
    [ -z "${CONFIG_SPT}" ] && return

    # If the host toolchain is NOT configured to build PIE exectuables by
    # default, assume it has no support for that and apply a workaround by
    # locating the spt tender starting at a virtual address of 1 GB.
    if ! cc_has_pie; then
        warn "Host toolchain does not build PIE executables, spt guest size will be limited to 1GB"
        warn "Consider upgrading to a Linux distribution with PIE support"
        CONFIG_SPT_NO_PIE=1
    fi

    if ! command -v pkg-config >/dev/null; then
        die "pkg-config is required"
    fi
    if ! pkg-config libseccomp; then
        die "libseccomp development headers are required"
    else
        if ! pkg-config --atleast-version=2.3.3 libseccomp; then
            # TODO Make this a hard error once there are no distros with
            # libseccomp < 2.3.3 in the various CIs.
            warn "libseccomp >= 2.3.3 is required for correct spt operation"
            warn "Proceeding anyway, expect tests to fail"
        elif ! pkg-config --atleast-version=2.4.1 libseccomp; then
            warn "libseccomp < 2.4.1 has known vulnerabilities"
            warn "Proceeding anyway, but consider upgrading"
        fi
        MAKECONF_LIBSECCOMP_CFLAGS="$(pkg-config --cflags libseccomp)"
        MAKECONF_LIBSECCOMP_LDLIBS="$(pkg-config --libs libseccomp)"
    fi
    if ! PKG_CFLAGS="${MAKECONF_LIBSECCOMP_CFLAGS}" gcc_check_header seccomp.h; then
        die "Could not compile with seccomp.h"
    fi
    if [ -n "${MAKECONF_LIBSECCOMP_LDLIBS}" ]; then
        if ! gcc_check_lib ${MAKECONF_LIBSECCOMP_LDLIBS}; then
            die "Could not link with ${MAKECONF_LIBSECCOMP_LDLIBS}"
        fi
    fi
}

config_host_freebsd()
{
    cc_is_clang || die "Only 'clang' is supported on FreeBSD"
    [ "${CONFIG_ARCH}" = "x86_64" ] ||
        die "Only 'x86_64' is supported on FreeBSD"

    # On FreeBSD/clang we use -nostdlibinc which gives us access to the
    # clang-provided headers for compiler instrinsics. We copy the rest
    # (std*.h, float.h and their dependencies) from the host.
    INCDIR=/usr/include
    SRCS="float.h stddef.h stdint.h stdbool.h stdarg.h"
    DEPS="$(mktemp)"
    get_header_deps ${INCDIR} ${SRCS} >${DEPS} || \
        die "Failure getting dependencies of host headers"
    # cpio will fail if CRT_INCDIR is below a symlink, so squash that
    mkdir -p ${CRT_INCDIR}
    CRT_INCDIR="$(readlink -f ${CRT_INCDIR})"
    (cd ${INCDIR} && cpio --quiet -Lpdm ${CRT_INCDIR} <${DEPS}) || \
        die "Failure copying host headers"
    rm ${DEPS}

    # Stack smashing protection:
    #
    # FreeBSD toolchains use a global (non-TLS) __stack_chk_guard by
    # default on x86_64, so there is nothing special we need to do here.
    MAKECONF_CFLAGS="${MAKECONF_CFLAGS} -nostdlibinc"

    # enable capsicum(4) sandbox if FreeBSD kernel is new enough
    [ "$(uname -K)" -ge 1200086 ] && CONFIG_HVT_FREEBSD_ENABLE_CAPSICUM=1
}

config_host_openbsd()
{
    cc_is_clang || die "Only 'clang' is supported on OpenBSD"
    [ "${CONFIG_ARCH}" = "x86_64" ] ||
        die "Only 'x86_64' is supported on OpenBSD"
    if ! ld_is_lld; then
        LD='/usr/bin/ld.lld'
        warn "Using GNU 'ld' is not supported on OpenBSD"
        warn "Falling back to 'ld.lld'"
        [ -e ${LD} ] || die "/usr/bin/ld.lld does not exist"
    fi

    # On OpenBSD/clang we use -nostdlibinc which gives us access to the
    # clang-provided headers for compiler instrinsics. We copy the rest
    # (std*.h, cdefs.h and their dependencies) from the host.
    INCDIR=/usr/include
    SRCS="float.h stddef.h stdint.h stdbool.h stdarg.h"
    DEPS="$(mktemp)"
    get_header_deps ${INCDIR} ${SRCS} >${DEPS} || \
        die "Failure getting dependencies of host headers"
    # cpio will fail if CRT_INCDIR is below a symlink, so squash that
    mkdir -p ${CRT_INCDIR}
    CRT_INCDIR="$(readlink -f ${CRT_INCDIR})"
    (cd ${INCDIR} && cpio -Lpdm ${CRT_INCDIR} <${DEPS}) || \
        die "Failure copying host headers"
    rm ${DEPS}

    MAKECONF_CFLAGS="${MAKECONF_CFLAGS} -mno-retpoline -fno-ret-protector -nostdlibinc"
    MAKECONF_LDFLAGS="${MAKECONF_LDFLAGS} -nopie"
}

# Allow external override of CC and LD.
CC=${CC:-cc}
LD=${LD:-ld}

CC_MACHINE=$(${CC} -dumpmachine)
[ $? -ne 0 ] &&
    die "Could not run '${CC} -dumpmachine', is your compiler working?"

# Determine CONFIG_HOST, CONFIG_ARCH from master matrix of supported build
# toolchain combinations (CC_MACHINE). Sets AVAILABLE_TARGETS and
# CONFIG_TARGETS (default list of targets to enable if none specified).
case ${CC_MACHINE} in
    x86_64-*linux*)
        CONFIG_ARCH=x86_64 CONFIG_HOST=Linux
        CONFIG_LD_MAX_PAGE_SIZE=0x1000
        AVAILABLE_TARGETS="hvt,spt,virtio,muen,xen"
        CONFIG_TARGETS="${AVAILABLE_TARGETS}"
        ;;
    aarch64-*linux*)
        CONFIG_ARCH=aarch64 CONFIG_HOST=Linux
        CONFIG_LD_MAX_PAGE_SIZE=0x1000
        AVAILABLE_TARGETS="hvt,spt"
        CONFIG_TARGETS="${AVAILABLE_TARGETS}"
        ;;
    powerpc64le-*linux*|ppc64le-*linux*)
        CONFIG_ARCH=ppc64le CONFIG_HOST=Linux
        CONFIG_LD_MAX_PAGE_SIZE=0x10000
        AVAILABLE_TARGETS="spt"
        CONFIG_TARGETS="${AVAILABLE_TARGETS}"
        ;;
    x86_64-*freebsd*)
        CONFIG_ARCH=x86_64 CONFIG_HOST=FreeBSD
        CONFIG_LD_MAX_PAGE_SIZE=0x1000
        AVAILABLE_TARGETS="hvt,virtio,muen,xen"
        CONFIG_TARGETS="${AVAILABLE_TARGETS}"
        ;;
    amd64-*openbsd*)
        CONFIG_ARCH=x86_64 CONFIG_HOST=OpenBSD
        CONFIG_LD_MAX_PAGE_SIZE=0x1000
        AVAILABLE_TARGETS="hvt,virtio,muen,xen"
        CONFIG_TARGETS="${AVAILABLE_TARGETS}"
        ;;
    *)
        die "Unsupported toolchain target: ${CC_MACHINE}"
        ;;
esac

# Arguments: Comma-separated list of TARGETS to enable.  Sets CONFIG_TARGET=1
# for each TARGET that is enabled, dies with an error if any TARGET is not
# supported on the current CONFIG_HOST/CONFIG_ARCH.
enable_targets()
{
    OLDIFS=${IFS}
    IFS=,
    for target in $@; do
        if echo ",${AVAILABLE_TARGETS}," | grep -q ",${target},"; then
            tmp="$(echo ${target} | tr '[a-z]' '[A-Z]')"
            eval "CONFIG_${tmp}=1"
        else
            die "--enable-targets: Target '${target}' is not available on ${CONFIG_HOST}/${CONFIG_ARCH}"
        fi
    done
    IFS=${OLDIFS}
}

MAKECONF_PREFIX=/usr/local
while [ $# -gt 0 ]; do
    OPT="$1"

    case "${OPT}" in
        --enable-targets=*)
            CONFIG_TARGETS="${OPT##*=}"
            ;;
        --prefix=*)
            MAKECONF_PREFIX="${OPT##*=}"
            ;;
        --help)
            usage
            ;;
        *)
            err "Unknown option: '${OPT}'"
            usage
            ;;
    esac

    shift
done

[ "${CONFIG_TARGETS}" = "force-all" ] && \
    CONFIG_TARGETS="hvt,spt,virtio,muen,xen"
[ "${CONFIG_TARGETS}" = "none" ] && \
    CONFIG_TARGETS=
enable_targets "${CONFIG_TARGETS}"

# C runtime header files appropriated from the host toolchain are installed
# here at configure time.
CRT_INCDIR=${PWD}/include/crt

MAKECONF_CFLAGS="-ffreestanding -fstack-protector-strong"
MAKECONF_LDFLAGS="-nostdlib -z max-page-size=${CONFIG_LD_MAX_PAGE_SIZE} -static"
MAKECONF_LIBSECCOMP_CFLAGS=
MAKECONF_LIBSECCOMP_LDLIBS=
CONFIG_SPT_NO_PIE=
CONFIG_HVT_FREEBSD_ENABLE_CAPSICUM=

case "${CONFIG_HOST}" in
    Linux)
        config_host_linux
        ;;
    FreeBSD)
        config_host_freebsd
        ;;
    OpenBSD)
        config_host_openbsd
        ;;
    *)
        die "Unsupported build OS: ${CONFIG_HOST}"
        ;;
esac

#
# Generate Makeconf, to be included by Makefiles.
#
cat <<EOM >Makeconf
# Generated by configure.sh, using CC=${CC} for target ${CC_MACHINE}
CONFIG_ARCH=${CONFIG_ARCH}
CONFIG_HOST=${CONFIG_HOST}
CONFIG_HVT=${CONFIG_HVT}
CONFIG_HVT_FREEBSD_ENABLE_CAPSICUM=${CONFIG_HVT_FREEBSD_ENABLE_CAPSICUM}
CONFIG_SPT=${CONFIG_SPT}
CONFIG_SPT_NO_PIE=${CONFIG_SPT_NO_PIE}
CONFIG_VIRTIO=${CONFIG_VIRTIO}
CONFIG_MUEN=${CONFIG_MUEN}
CONFIG_XEN=${CONFIG_XEN}
MAKECONF_PREFIX=${MAKECONF_PREFIX}
MAKECONF_CC=${CC}
MAKECONF_LD=${LD}
MAKECONF_CFLAGS=${MAKECONF_CFLAGS}
MAKECONF_LDFLAGS=${MAKECONF_LDFLAGS}
MAKECONF_LIBSECCOMP_CFLAGS=${MAKECONF_LIBSECCOMP_CFLAGS}
MAKECONF_LIBSECCOMP_LDLIBS=${MAKECONF_LIBSECCOMP_LDLIBS}
EOM

#
# Generate Makeconf.sh, to be included by shell scripts.
#
sed -Ee 's/^([A-Z_]+)=(.*)$/\1="\2"/' Makeconf >Makeconf.sh

#
# Generate solo5-config.sh, to be called by downstream build systems.
#
sed -e '/@@MAKECONF_SH@@/ r Makeconf.sh' \
    -e '/@@MAKECONF_SH@@/ d' \
    -e "s/@@AVAILABLE_TARGETS@@/${AVAILABLE_TARGETS}/g" \
    scripts/solo5-config.sh.in >solo5-config.sh
chmod +x solo5-config.sh

#
# Done.
#
echo "${prog_NAME}: Configured for ${CC_MACHINE}."
echo "${prog_NAME}: Enabled targets: ${CONFIG_TARGETS}."
