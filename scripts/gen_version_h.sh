#!/bin/sh

[ $# -ne 1 ] && exit 1
OUTPUT="$1"
GIT_VERSION="$(git -C . describe --dirty --tags --always)" || exit 1

cat <<EOM >${OUTPUT}.tmp || exit 1
/* Automatically generated, do not edit */

#ifndef __VERSION_H__
#define __VERSION_H__

#define SOLO5_VERSION "${GIT_VERSION}"

#endif
EOM

# Only touch the target file if it does not exist yet or Git version differs.
if [ -f ${OUTPUT} ] && diff -q ${OUTPUT} ${OUTPUT}.tmp >/dev/null; then
    rm ${OUTPUT}.tmp
else
    mv ${OUTPUT}.tmp ${OUTPUT}
fi
