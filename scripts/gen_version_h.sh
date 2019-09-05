#!/bin/sh

die ()
{
    [ $# -ne 0 ] && echo $0: error: "$@" 1>&2
    exit 1
}


[ $# -ne 1 ] && exit 1
VERSION_H="$1"

# If we are being run from a release tarball, then a version.h.distrib must
# be present, and we always use that as our source of truth.
if [ ! -d "./.git" ]; then
    if [ ! -f "${VERSION_H}.distrib" ]; then
	die "${VERSION_H}.distrib: Not found, and we are not in a Git tree"
    fi
    cp "${VERSION_H}.distrib" "${VERSION_H}" || die
    exit 0
fi

# Otherwise, use "git describe" to get the exact version of this tree, and
# generate a version.h from it.
GIT_VERSION="$(git -C . describe --dirty --tags --always)" ||
    die "Could not determine Git version"

cat <<EOM >${VERSION_H}.tmp || die
/* Automatically generated, do not edit */

#ifndef __VERSION_H__
#define __VERSION_H__

#define SOLO5_VERSION "${GIT_VERSION}"

#endif
EOM

# Only touch the target file if it does not exist yet or Git version differs.
if [ -f ${VERSION_H} ] && diff -q ${VERSION_H} ${VERSION_H}.tmp >/dev/null; then
    rm ${VERSION_H}.tmp || die
else
    mv ${VERSION_H}.tmp ${VERSION_H} || die
fi
