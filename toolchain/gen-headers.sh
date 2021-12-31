#!/bin/sh
# Copyright (c) 2015-2021 Contributors as noted in the AUTHORS file
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

# gen-headers.sh: Appropriate internal "C runtime" header files needed for the
# target toolchain from the origin compiler.

prog_NAME="$(basename $0)"

cleanup()
{
    rm -f conftmp.c conftmp.d conftmp*.o
}

die()
{
    echo "${prog_NAME}: ERROR: $@" 1>&2
    cleanup
    exit 1
}

cc_is_clang()
{
    ${CC} -dM -E - </dev/null | grep -Eq '^#define __clang__ 1$'
}

# Arguments: PATH, FILES...
# For the header FILES..., all of which must be relative to PATH, resolve their
# dependencies using the C preprocessor and output a list of FILES... plus all
# their unique dependencies, also relative to PATH.
cc_get_header_deps()
{
    temp="$PWD/conftmp.d"
    local path="$1"
    shift
    (
        cd ${path} || return 1
        ${CC} -M "$@" >${temp} || return 1
        sed -e 's!.*\.o:!!g' -e "s!${path}/!!g" ${temp} \
            | tr ' \\' '\n' \
            | sort \
            | uniq
        rm ${temp}
    )
}

[ "$#" -ne 1 ] && die "Missing DESTDIR"
DESTDIR=$1
. ../Makeconf.sh || die "Can't find ../Makeconf.sh"

mkdir -p ${DESTDIR} || die "mkdir failed"

if CC=${CONFIG_TARGET_CC} cc_is_clang; then
    case ${CONFIG_HOST} in
        # The BSDs don't ship some standard headers that we need in Clang's
        # resource directory. Appropriate these from the host system.
        FreeBSD|OpenBSD)
            SRCDIR=/usr/include
            SRCS="float.h stddef.h stdint.h stdbool.h stdarg.h"
            DEPS="$(mktemp)"
            CC=${CONFIG_TARGET_CC} cc_get_header_deps ${SRCDIR} ${SRCS} \
                >${DEPS} || \
                die "Failure getting dependencies of host headers"
            # cpio will fail if CRT_INCDIR is below a symlink, so squash that
            DESTDIR="$(readlink -f ${DESTDIR})"
            Q=
            [ "${CONFIG_HOST}" = "FreeBSD" ] && Q="--quiet"
            (cd ${SRCDIR} && cpio ${Q} -Lpdm ${DESTDIR} <${DEPS}) || \
                die "Failure copying host headers"
            rm ${DEPS}
            ;;
        # Other known Clang toolchains don't require anything special here as
        # -nostdlibinc will pick up all we need from the compiler's resource
        # directory.
        *)
            ;;
    esac
else
    # For GCC there isn't an equivalent of -nostdlibinc, so we need to
    # appropriate all of its internal headers.
    SRCDIR="$(${CONFIG_TARGET_CC} -print-file-name=include)"
    [ -d "${SRCDIR}" ] || die "Cannot determine gcc include directory"
    cp -R "${SRCDIR}/." ${DESTDIR} || \
        die "Failure copying host headers"
fi

cleanup
