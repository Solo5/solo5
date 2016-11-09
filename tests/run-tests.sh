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

TESTS="test_hello.ukvm test_hello.virtio \
    test_blk.ukvm:d test_blk.virtio:d \
    test_ping_serve.ukvm:n test_ping_serve.virtio:n"

FAILED=
SKIPPED=
for T in ${TESTS}; do
    NAME=${T%:*}
    echo -n "${NAME}: "
    run ${T}
    case $? in
    0)
        STATUS=0
        RESULT="${TGREEN}PASSED${TOFF}"
        ;;
    99)
        STATUS=1
        RESULT="${TRED}FAILED${TOFF}"
        ;;
    98)
        STATUS=0
        RESULT="${TYELL}SKIPPED${TOFF}"
        SKIPPED=1
        ;;
    124)
        STATUS=1
        RESULT="${TRED}TIMEOUT${TOFF}"
        ;;
    *)
        STATUS=1
        RESULT="${TRED}ERROR${TOFF}"
        ;;
    esac
    echo ${RESULT}
    if [ ${STATUS} -ne 0 ]; then
        cat ${TMPDIR}/${NAME}.log.* | sed "s/^/${TRED}>${TOFF} /"
        FAILED=1
    fi
done

if [ -n "${FAILED}" ]; then
    echo "Overall status: ${TRED}FAILURE${TOFF}"
    exit 1
else
    if [ -n "${SKIPPED}" ]; then
        echo "Overall status: ${TYELL}SUCCESS${TYELL}"
    else
        echo "Overall status: ${TGREEN}SUCCESS${TGREEN}"
    fi
    exit 0
fi
