#!/bin/sh

[ $(id -u) -ne 0 ] && exit 1

ip tuntap add tap100 mode tap
ip addr add 10.0.0.1/24 dev tap100
ip link set dev tap100 up

cd tests && ./run-tests.sh
