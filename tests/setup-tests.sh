#!/bin/sh
#
# Set up test environment.
#
# Convention is: tap interface named 'tap100', host address of 10.0.0.1/24.
#

if [ $(id -u) -ne 0 ]; then
    echo "$0: must be root" 1>&2
    exit 1
fi

case `uname -s` in
Linux)
    set -xe
    ip tuntap add tap100 mode tap
    ip addr add 10.0.0.1/24 dev tap100
    ip link set dev tap100 up
    ;;
FreeBSD)
    kldload vmm
    kldload if_tap
    kldload nmdm
    sysctl -w net.link.tap.up_on_open=1
    ifconfig tap100 create 10.0.0.1/24 link0 up
    ;;
*)
    exit 1
    ;;
esac
