#!/bin/sh

die ()
{
    [ $# -ne 0 ] && echo $0: error: "$@" 1>&2
    exit 1
}


[ $# -ne 1 ] && exit 1
VERSION_H="$1"

# Determine the version, in order of precedence:
#   1. "git describe" in a real checkout (allow ./.git to be a file for worktrees)
#   2. version.h.distrib shipped in a release tarball
#   3. $SOLO5_VERSION for an exported snapshot (e.g. an opam tarball pin) that has
#      neither a Git tree nor a version.h.distrib
if [ -d "./.git" -o -f "./.git" ]; then
    VERSION="$(git -C . describe --dirty --tags --always)" ||
	die "Could not determine Git version"
elif [ -f "${VERSION_H}.distrib" ]; then
    cp "${VERSION_H}.distrib" "${VERSION_H}" || die
    exit 0
elif [ -n "${SOLO5_VERSION}" ]; then
    VERSION="${SOLO5_VERSION}"
else
    die "not a Git tree, ${VERSION_H}.distrib not found, and SOLO5_VERSION unset"
fi

cat <<EOM >${VERSION_H}.tmp || die
/* Automatically generated, do not edit */

#ifndef __VERSION_H__
#define __VERSION_H__

#define SOLO5_VERSION "${VERSION}"

#endif
EOM

# Only touch the target file if it does not exist yet or the version differs.
if [ -f ${VERSION_H} ] && diff -q ${VERSION_H} ${VERSION_H}.tmp >/dev/null; then
    rm ${VERSION_H}.tmp || die
else
    mv ${VERSION_H}.tmp ${VERSION_H} || die
fi
