#!/bin/sh
#
# THIS IS NOT THE BUILD SCRIPT FOR Solo5.
#
# This script is used for automated builds with surf-build.
# See https://github.com/Solo5/solo5-ci for details.
#
if type gmake >/dev/null 2>&1; then
    MAKE=gmake
else
    MAKE=make
fi

set -xe
uname -a
cc --version
${MAKE}
# Some CIs can now run tests, so do that.
if [ -n "${SURF_RUN_TESTS}" ]; then
    # XXX grub-bhyve is unstable under nested virt, so don't run the
    # virtio tests on FreeBSD.
    if [ "$(uname -s)" = "FreeBSD" ]; then
        echo BUILD_VIRTIO=no >>Makeconf
    fi
    sudo tests/setup-tests.sh
    sudo tests/run-tests.sh
fi
