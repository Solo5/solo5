#!/bin/sh
# Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
#
# This file is part of Solo5, a unikernel base layer.
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

#
# Test harness for running Solo5 automated tests.
#
# Tries to run as many tests as are available. Skips tests which cannot be run
# due to lack of privilege or hypervisor on the host.
#
# For best (most complete) results run as root.
#
# TODO This should probably be rewritten in something other than shell.

die ()
{
    echo $0: "$@" 1>&2
    exit 1
}

nuketmpdir ()
{
    [ -n "${PRESERVE_TMPDIR}" ] && return
    [ -z "${TMPDIR}" ] && return
    [ ! -d "${TMPDIR}" ] && return
    rm -rf ${TMPDIR}
}

trap nuketmpdir 0 INT TERM
TMPDIR=$(mktemp -d)
if [ $? -ne 0 ]; then
    echo "error creating temporary directory" 1>&2
    exit 1
fi

# Usage:
#     run_test [ OPTIONS ] TEST
# Returns:
#     0: Success
#     98: Skipped
#     99: Failed (ran to completion but no SUCCESS/ABORT in logs)
#     124: Timeout
#     (anything else): Other error
run_test ()
{
    logto ()
    {
        LOG=${TMPDIR}/$1
        exec >>${LOG} 2>&1 </dev/null
    }

    local ARGS
    local DISK
    local NET
    local WANT_ABORT
    local NAME
    local UNIKERNEL
    local TEST_DIR
    local STATUS

    ARGS=$(getopt dnav $*)
    [ $? -ne 0 ] && die "Invalid test options"
    set -- ${ARGS}
    DISK=
    NET=
    WANT_ABORT=
    WANT_QUIET=
    while true; do
        case "$1" in
        -d)
            DISK=${TMPDIR}/disk.img
            shift
            ;;
        -n)
            NET=tap100
            NET_IP=10.0.0.2
            shift
            ;;
        -a)
            # This test must ABORT
            WANT_ABORT=true
            shift
            ;;
        -v)
            # This test must run in quiet mode
            WANT_QUIET=true
            shift;
            ;;
        --)
            shift; break
            ;;
        esac
    done
    [ $# -lt 1 ] && die "Missing operand"
    UNIKERNEL=${SCRIPT_DIR}/${1%%.*}/$1
    TEST_DIR=${SCRIPT_DIR}/${1%%.*}
    NAME=$1
    [ ! -d ${TEST_DIR} ] && die "Could not determine directory for ${NAME}"
    [ ! -x ${UNIKERNEL} ] && die "Not found: ${UNIKERNEL}"
    shift

    # Test requires a block device. Create a fresh disk image for it.
    if [ -n "${DISK}" ]; then
        (
            logto ${NAME}.log.1
            set -x; dd if=/dev/zero of=${TMPDIR}/disk.img bs=4k count=1024
        )
    fi

    # Network test. Run flood ping as the "client".
    # XXX This is pretty hacky, and needs improvement. Should also check for
    # no packet loss on the ping side.
    if [ -n "${NET}" ]; then
        # Need root to run this test (for ping -f)
        [ $(id -u) -ne 0 ] && return 98
        NET=tap100
        (
            logto ${NAME}.log.2
            sleep 1
            set -x; timeout 30s ping -fq -c 100000 ${NET_IP}
        ) &
        PID_PING=$!
    fi

    # Run the unikernel under test.
    (
        logto ${NAME}.log.0

        echo "-------- ${NAME} START --------"

        case ${NAME} in
        *.ukvm)
            OS="$(uname -s)"
            case ${OS} in
                Linux)
                    [ -c /dev/kvm -a -w /dev/kvm ] || exit 98
                    ;;
                FreeBSD)
                    # TODO, just try and run the test anyway
                    ;;
                *)
                    die "Don't know how to run ${NAME} on ${OS}"
                    ;;
            esac
            UKVM=${TEST_DIR}/ukvm-bin
            [ -n "${DISK}" ] && UKVM="${UKVM} --disk=${DISK}"
            [ -n "${NET}" ] && UKVM="${UKVM} --net=${NET}"
            (set -x; timeout 30s ${UKVM} -- ${UNIKERNEL} "$@")
            STATUS=$?
            ;;
        *.virtio)
            VIRTIO=${SCRIPT_DIR}/../tools/run/solo5-run-virtio.sh
            [ -n "${DISK}" ] && VIRTIO="${VIRTIO} -d ${DISK}"
            [ -n "${NET}" ] && VIRTIO="${VIRTIO} -n ${NET}"
            (set -x; timeout 30s ${VIRTIO} -- ${UNIKERNEL} "$@")
            STATUS=$?
            ;;
        esac
        
        echo "-------- ${NAME} END (status ${STATUS}) --------"
        exit ${STATUS}
    )

    STATUS=$?
    LOGS=$(find ${TMPDIR} -type f -name ${NAME}.log.\*)

    if [ -z "${LOGS}" ]; then

        wait
        return 99
    fi

    case ${NAME} in
    *.ukvm)
        case ${STATUS} in
        0|255) 
            if   [ -z "${WANT_ABORT}" ] && [ "${STATUS}" -eq "0" ]; then
                STATUS=0
            elif [ -n "${WANT_ABORT}" ] && [ "${STATUS}" -eq "255" ]; then
                STATUS=0
            else
                STATUS=99
            fi
            ;;
        esac
        ;;

    *.virtio)
        case ${STATUS} in
        # XXX Should this be abstracted out in solo5-run-virtio.sh?
        0|2|83) 
            STATUS=99
            if [ -z "${WANT_ABORT}" ]; then
                grep -q SUCCESS ${LOGS} && STATUS=0
            else
                grep -q ABORT ${LOGS} && STATUS=0
            fi
            if [ ${STATUS} -eq "0" ]; then
                if [ -n "${WANT_QUIET}" ]; then
                    # XXX This is all just horribly fragile.
                    # If "quiet mode" was requested, there should not be any output
                    # on the console with "Solo5:" in it unless there was an error,
                    # with the exception of "Solo5: Halted" on non-QEMU hypervisors.
                    if sed -e '/^Solo5: Halted$/d' ${LOGS} | grep -q 'Solo5:'; then
                        STATUS=99
                    fi
                fi
            fi
            ;;
        esac
        ;;
    esac

    wait

    return ${STATUS}
}

dumplogs ()
{
    LOGS=$(find ${TMPDIR} -type f -name $1.log.\*)
    for F in ${LOGS}; do
        echo "$2${F}: $3"
        cat ${F} | sed "s/^/$2>$3 /"
    done
}

add_test ()
{
    TESTS="${TESTS} $@"
}

ARGS=$(getopt v $*)
[ $? -ne 0 ] && exit 1
set -- $ARGS
VERBOSE=
while true; do
    case "$1" in
    -v)
        VERBOSE=1
        shift
        ;;
    --)
        shift; break
        ;;
    esac
done

if [ -t 1 ]; then
    TRED=$(tput setaf 1)
    TGREEN=$(tput setaf 2)
    TYELL=$(tput setaf 3)
    TOFF=$(tput sgr0)
else
    TRED=
    TGREEN=
    TYELL=
    TOFF=
fi

SCRIPT_DIR=$(readlink -f $(dirname $0))

# Grab variables from Makeconf to determine which targets have been built.
MAKECONF=${SCRIPT_DIR}/../Makeconf
[ ! -f ${MAKECONF} ] && die "Can't find Makeconf, looked in ${MAKECONF}"
eval $(grep -E ^BUILD_.+=.+ ${MAKECONF})

#
# List of tests to run is defined here.
#
# Syntax: add_test test_foo.TARGET[/[OPTIONS]/[ARGS]]
#
TESTS=
if [ "${BUILD_UKVM}" = "yes" ]; then
    add_test test_hello.ukvm//Hello_Solo5
    add_test test_quiet.ukvm/-v/--solo5:quiet
    add_test test_globals.ukvm
    add_test test_exception.ukvm/-a
    add_test test_fpu.ukvm
    add_test test_time.ukvm
    add_test test_blk.ukvm/-d
    add_test test_ping_serve.ukvm/-n/limit
fi
if [ "${BUILD_VIRTIO}" = "yes" ]; then
    add_test test_hello.virtio//Hello_Solo5
    add_test test_quiet.virtio/-v/--solo5:quiet
    add_test test_globals.virtio
    add_test test_exception.virtio/-a
    add_test test_fpu.virtio
    add_test test_time.virtio
    add_test test_blk.virtio/-d
    add_test test_ping_serve.virtio/-n/limit
fi
# No tests for BUILD_MUEN (yet).

echo "--------------------------------------------------------------------------------"
echo "Starting tests at $(date)"

FAILED=
SKIPPED=
for T in ${TESTS}; do
    OLDIFS=$IFS
    IFS=/
    set -- ${T}
    IFS=$OLDIFS
    [ $# -lt 1 -o $# -gt 3 ] && die "Error in test specification: '${T}'"
    NAME=$1; shift
    if [ -n "$1" ]; then
        OPTS=$1; shift
    else
        OPTS=
    fi
    printf "%-32s: " "${NAME}"
    run_test ${OPTS} -- ${NAME} "$@"
    case $? in
    0)
        STATUS=0
        echo "${TGREEN}PASSED${TOFF}"
        [ -n "${VERBOSE}" ] && dumplogs ${NAME} ${TGREEN} ${TOFF}
        ;;
    1|99)
        STATUS=1
        echo "${TRED}FAILED${TOFF}"
        ;;
    98)
        STATUS=0
        SKIPPED=1
        echo "${TYELL}SKIPPED${TOFF}"
        ;;
    124)
        STATUS=1
        echo "${TRED}TIMEOUT${TOFF}"
        ;;
    *)
        STATUS=1
        echo "${TRED}ERROR${TOFF}"
        ;;
    esac
    if [ ${STATUS} -ne 0 ]; then
        FAILED=1
        dumplogs ${NAME} ${TRED} ${TOFF}
    fi
done

echo "Finished tests at $(date)"
echo "--------------------------------------------------------------------------------"

if [ -n "${FAILED}" ]; then
    RESULT="${TRED}FAILURE${TOFF}"
    STATUS=1
else
    # Anything skipped gives a yellow "mostly SUCCESS".
    if [ -n "${SKIPPED}" ]; then
        RESULT="${TYELL}SUCCESS${TOFF}"
    else
        RESULT="${TGREEN}SUCCESS${TOFF}"
    fi
    STATUS=0
fi
printf "%-32s: %s\n" "Result" "${RESULT}"
exit ${STATUS}
