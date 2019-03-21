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

if [ -z "${SURF_SUDO}" ]; then
    SURF_SUDO=sudo
fi

message()
{
    printf %80s\\n | tr " " "\*"
    echo "$@"
    printf %80s\\n | tr " " "\*"
}

do_info()
{
    message "System information:"
    uname -a
    echo "Compiler version:"
    cc --version
    echo "Compiler version detail:"
    cc -v
}

try()
{
    if ( eval "$@" ); then
        :
    else
        echo "FAILURE: '$@' failed: Status: $?"
        exit 1
    fi
}

do_basic()
{
    message "Starting build: '${MAKE}'"
    try ${MAKE}
    # Some CIs can now run tests, so do that.
    if [ -n "${SURF_RUN_TESTS}" ]; then
        message "Running tests:"
        # XXX grub-bhyve is unstable under nested virt, so don't run the
        # virtio tests on FreeBSD.
        if [ "$(uname -s)" = "FreeBSD" ]; then
            echo BUILD_VIRTIO=no >>Makeconf
        fi
        try ${SURF_SUDO} tests/setup-tests.sh
        try ${SURF_SUDO} tests/run-tests.sh
    fi
}

do_e2e()
{
    message "Would run E2E here"
    :
}

do_info
case "${SURF_BUILD_TYPE}" in
    basic)
        do_basic
        ;;
    e2e)
        do_e2e
        ;;
    *)
        echo "WARNING: SURF_BUILD_TYPE not set, assuming 'basic'"
        do_basic
        ;;
esac

message "Success!"
# NOTE: Failure exit status is returned in try(), if we got here then the build
# NOTE: completed.
