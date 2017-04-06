#!/bin/sh
# kickme
if type gmake >/dev/null 2>&1; then
    MAKE=gmake
else
    MAKE=make
fi

set -xe
${MAKE}
# Don't run any tests yet
