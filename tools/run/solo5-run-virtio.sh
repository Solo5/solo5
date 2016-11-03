#!/bin/sh
usage ()
{
    cat <<EOM 1>&2
Usage: solo5-run-virtio [ OPTIONS ] UNIKERNEL [ -- ] [ ARGUMENTS ... ]

Launch the Solo5 UNIKERNEL (virtio target). Unikernel output is sent to stdout.

Options:
    -d DISK: Attach virtio-blk device with DISK image file.

    -m MEM: Start guest with MEM megabytes of memory (default is 128).

    -n NETIF: Attach virtio-net device with NETIF tap interface.

    -q: Quiet mode. Don't print hypervisor incantations.

    -H HV: Use hypervisor HV (default is "best available").
EOM
    exit 1
}

die ()
{
    echo solo5-run-virtio: error: "$@" 1>&2
    exit 1
}

hv_addargs ()
{
    if [ -z "${HVCMD}" ]; then
        HVCMD="$@"
    else
        HVCMD="${HVCMD} $@"
    fi
}

is_quiet ()
{
    [ -n "${QUIET}" ]
}

# Parse command line arguments.
ARGS=$(getopt d:m:n:qH: $*)
[ $? -ne 0 ] && usage
set -- $ARGS
MEM=128
HV=best
NETIF=
BLKIMG=
QUIET=
while true; do
    case "$1" in
    -d)
        BLKIMG=$(readlink -f $2)
        [ -f ${BLKIMG} ] || die "not found: ${BLKIMG}"
        shift; shift
        ;;
    -m)
        MEM="$2"
        shift; shift
        ;;
    -n)
        NETIF="$2"
        ifconfig ${NETIF} >/dev/null || die "no such network interface: ${NETIF}"
        shift; shift
        ;;
    -q)
        QUIET=1
        shift
        ;;
    -H)
        HV="$2"
        shift; shift
        ;;
    --)
        shift; break
        ;;
    esac
done
[ $# -lt 1 ] && usage

UNIKERNEL=$(readlink -f $1)
shift
[ ! -f ${UNIKERNEL} ] && die "not found: ${UNIKERNEL}"
VMNAME=$(basename ${UNIKERNEL})

if [ "${HV}" = "best" ]; then
    SYS=$(uname -s)
    case ${SYS} in
    Linux)
        if [ -c /dev/kvm -a -w /dev/kvm ]; then
            HV=kvm
        else
            HV=qemu
        fi
        ;;
    FreeBSD)
        # XXX How to detect if bhyve is available on this machine?
        HV=bhyve
        [ $(id -u) -eq 0 ] || die "Root privileges required for bhyve"
        type grub-bhyve >/dev/null 2>&1 \
            || die "Please install grub-bhyve from ports"
        ;;
    *)
        die "unsupported os: ${SYS}"
        ;;
    esac
fi

case ${HV} in
kvm|qemu)
    hv_addargs qemu-system-x86_64
    case ${HV} in
    kvm)
        hv_addargs -cpu host -enable-kvm
        ;;
    qemu)
        hv_addargs -cpu Westmere
        ;;
    esac
    hv_addargs -m ${MEM}

    # Kill all default devices provided by QEMU, we don't need them.
    hv_addargs -nodefaults -no-acpi

    # Console. We could use just "-nograhic -vga none", however that MUXes the
    # QEMU monitor on stdio which requires ^Ax to exit. This makes things look
    # more like a normal process (quit with ^C), consistent with bhyve and ukvm.
    hv_addargs -display none -serial stdio

    # Network
    if [ -n "${NETIF}" ]; then
        hv_addargs -device virtio-net,netdev=n0
        hv_addargs -netdev tap,id=n0,ifname=${NETIF},script=no,downscript=no
    fi
    # Disk
    if [ -n "${BLKIMG}" ]; then
        hv_addargs -drive file=${BLKIMG},if=virtio,format=raw
    fi

    hv_addargs -kernel ${UNIKERNEL}

    # QEMU command line parsing is just stupid.
    ARGS=
    if [ "$#" -ge 1 ]; then
        ARGS="$(echo "$@" | sed -e s/,/,,/g)"
        is_quiet || set -x
        exec ${HVCMD} -append "${ARGS}"
    else
        is_quiet || set -x
        exec ${HVCMD}
    fi
    ;;
bhyve)
    # Just blow away any pre-existing VM of the same name.
    bhyvectl --destroy --vm=${VMNAME} >/dev/null 2>&1
    # Load the VM using grub-bhyve. Kill stdout as this is normal GRUB output.
    (is_quiet || set -x; \
        printf "multiboot ${UNIKERNEL} placeholder %s\nboot\n" "$*" \
        | grub-bhyve -M ${MEM} "${VMNAME}" >/dev/null) \
        || die "Could not initialise VM"

    hv_addargs bhyve
    hv_addargs -m ${MEM}
    hv_addargs -H -s 0:0,hostbridge -s 1:0,lpc

    # Console. Bhyve insists on talking to a TTY so fake one in a way that we
    # can redirect output to a file (see below).
    TTYA=/dev/nmdm$$A
    TTYB=/dev/nmdm$$B
    hv_addargs -l com1,${TTYB}
    
    # Network
    if [ -n "${NETIF}" ]; then
        hv_addargs -s 2:0,virtio-net,${NETIF}
    fi
    # Disk
    if [ -n "${BLKIMG}" ]; then
        hv_addargs -s 3:0,virtio-blk,${BLKIMG}
    fi

    hv_addargs ${VMNAME}

    # Fake a console using nmdm(4). Open 'A' end first and keep it open, then
    # set some sane TTY parameters and launch bhyve on the 'B' end.
    # Bhyve and cat will respond to interactive SIGINT correctly.
    # XXX Can occasionally leak /dev/nmdm devices, oh well ...
    cat ${TTYA} &
    CAT=$!
    killcat ()
    {
        kill ${CAT}
    }
    trap killcat 0 INT TERM
    stty -f ${TTYB} >/dev/null
    stty -f ${TTYA} 115200 igncr

    # Can't do exec here since we need to clean up the 'cat' process.
    ( is_quiet || set -x; ${HVCMD} )
    exit $?
    ;;
best)
    die "Could not determine hypervisor, try with -H"
    ;;
*)
    die "Unknown hypervisor: ${HV}"
    ;;
esac
