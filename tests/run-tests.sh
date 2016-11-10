#!/bin/sh

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

run ()
{
    (
        NAME=${1%:*}
        if [ ${1##*:} = $1 ]; then
            OPTS=
        else
            OPTS=${1##*:}
        fi

        case ${OPTS} in
        *d*)
            (
                LOG=${TMPDIR}/${NAME}.log.1
                exec >>${LOG} 2<&1 </dev/null
                set -x; dd if=/dev/zero of=${TMPDIR}/disk.img bs=4k count=1024
            )
            DISK=${TMPDIR}/disk.img
            ;;
        *n*)
            # Need root to run this test (for ping -f)
            [ $(id -u) -ne 0 ] && exit 98
            NET=tap100
            (
                LOG=${TMPDIR}/${NAME}.log.2
                exec >>${LOG} 2<&1 </dev/null
                sleep 1
                set -x; timeout 30s ping -fq -c 100000 10.0.0.2
            ) &
            PID_PING=$!
            ;;
        esac
        
        LOG=${TMPDIR}/${NAME}.log.0
        exec >>${LOG} 2>&1 </dev/null

        echo "-------- ${NAME} START --------"

        case ${NAME} in
        *.ukvm)
            [ -c /dev/kvm -a -w /dev/kvm ] || exit 98
            UKVM=$(basename ${NAME} .ukvm)/ukvm-bin
            [ -n "${DISK}" ] && UKVM="${UKVM} --disk=${DISK}"
            [ -n "${NET}" ] && UKVM="${UKVM} --net=${NET}"
            T=$(basename ${NAME} .ukvm)/${NAME}
            (set -x; timeout 30s ${UKVM} ${T})
            STATUS=$?
            ;;
        *.virtio)
            VIRTIO=$(dirname $0)/../tools/run/solo5-run-virtio.sh
            [ -n "${DISK}" ] && VIRTIO="${VIRTIO} -d ${DISK}"
            [ -n "${NET}" ] && VIRTIO="${VIRTIO} -n ${NET}"
            T=$(basename ${NAME} .virtio)/${NAME}
            (set -x; timeout 30s ${VIRTIO} ${T})
            STATUS=$?
            ;;
        esac
        
        echo "-------- ${NAME} END (status ${STATUS}) --------"

        [ -n "${PID_PING}" ] && kill ${PID_PING}

        case ${STATUS} in
        0|83)
            if grep -q SUCCESS ${LOG}; then
                STATUS=0
            else
                STATUS=99
            fi
            ;;
        esac
        exit ${STATUS}
    )
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

dumplogs ()
{
    for F in ${TMPDIR}/${NAME}.log.*; do
        echo "$1${F}: $2"
        cat ${F} | sed "s/^/$1>$2 /"
    done
}

echo "--------------------------------------------------------------------------------"
echo "Starting tests at $(date)"

TESTS="test_hello.ukvm test_hello.virtio \
    test_blk.ukvm:d test_blk.virtio:d \
    test_ping_serve.ukvm:n test_ping_serve.virtio:n"

FAILED=
SKIPPED=
for T in ${TESTS}; do
    NAME=${T%:*}
    printf "%-32s: " "${NAME}"
    run ${T}
    case $? in
    0)
        STATUS=0
        echo "${TGREEN}PASSED${TOFF}"
        [ -n "${VERBOSE}" ] && dumplogs ${TGREEN} ${TOFF}
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
        dumplogs ${TRED} ${TOFF}
    fi
done

echo "Finished tests at $(date)"
echo "--------------------------------------------------------------------------------"

if [ -n "${FAILED}" ]; then
    RESULT="${TRED}FAILURE${TOFF}"
    STATUS=1
else
    if [ -n "${SKIPPED}" ]; then
        RESULT="${TYELL}SUCCESS${TOFF}"
    else
        RESULT="${TGREEN}SUCCESS${TOFF}"
    fi
    STATUS=0
fi
printf "%-32s: %s\n" "Result" "${RESULT}"
exit ${STATUS}
