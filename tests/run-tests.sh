#!/bin/sh
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
#     99: Failed (ran to completion but no SUCCESS in logs)
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
    local NAME
    local UNIKERNEL
    local TEST_DIR
    local STATUS

    ARGS=$(getopt dn $*)
    [ $? -ne 0 ] && return 1
    set -- ${ARGS}
    DISK=
    NET=
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
        --)
            shift; break
            ;;
        esac
    done
    [ $# -ne 1 ] && return 1
    UNIKERNEL=${SCRIPT_DIR}/${1%%.*}/$1
    TEST_DIR=${SCRIPT_DIR}/${1%%.*}
    NAME=$1
    [ ! -d ${TEST_DIR} ] && return 1
    [ ! -x ${UNIKERNEL} ] && return 1

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
            [ -c /dev/kvm -a -w /dev/kvm ] || exit 98
            UKVM=${TEST_DIR}/ukvm-bin
            [ -n "${DISK}" ] && UKVM="${UKVM} --disk=${DISK}"
            [ -n "${NET}" ] && UKVM="${UKVM} --net=${NET}"
            (set -x; timeout 30s ${UKVM} ${UNIKERNEL})
            STATUS=$?
            ;;
        *.virtio)
            VIRTIO=${SCRIPT_DIR}/../tools/run/solo5-run-virtio.sh
            [ -n "${DISK}" ] && VIRTIO="${VIRTIO} -d ${DISK}"
            [ -n "${NET}" ] && VIRTIO="${VIRTIO} -n ${NET}"
            (set -x; timeout 30s ${VIRTIO} ${UNIKERNEL})
            STATUS=$?
            ;;
        esac
        
        echo "-------- ${NAME} END (status ${STATUS}) --------"
        exit ${STATUS}
    )

    STATUS=$?
    case ${STATUS} in
    # XXX Should this be abstracted out in solo5-run-virtio.sh?
    0|2|83) 
        LOGS=$(find ${TMPDIR} -type f -name ${NAME}.log.\*)
        STATUS=99
        [ -n "${LOGS}" ] && grep -q SUCCESS ${LOGS} && STATUS=0
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
. ${MAKECONF}

#
# List of tests to run is defined here.
#
# Format: test_foo.TARGET[:OPTIONS]
#
TESTS=
if [ -n "${BUILD_UKVM}" ]; then
    TESTS="${TESTS} test_hello.ukvm test_blk.ukvm:-d test_ping_serve.ukvm:-n"
fi
if [ -n "${BUILD_VIRTIO}" ]; then
    TESTS="${TESTS} test_hello.virtio test_blk.virtio:-d test_ping_serve.virtio:-n"
fi

echo "--------------------------------------------------------------------------------"
echo "Starting tests at $(date)"

FAILED=
SKIPPED=
for T in ${TESTS}; do
    NAME=${T%:*}
    OPTS=${T#*:}
    [ "${OPTS}" = "${NAME}" ] && OPTS=
    printf "%-32s: " "${NAME}"
    run_test ${OPTS} ${NAME}
    case $? in
    0)
        STATUS=0
        echo "${TGREEN}PASSED${TOFF}"
        [ -n "${VERBOSE}" ] && dumplogs ${NAME} ${TGREEN} ${TOFF}
        ;;
    99)
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
