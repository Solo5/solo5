#!/bin/bash

TARGET=$1
MAC=52:54:00:12:34:56

if [[ "o$TARGET" == "oukvm" ]]; then
    echo "configuring network for Solo5 ukvm target"
    # remove old virtio network setup
    ip link del veth1

    # ukvm network setup
    #
    # 52:54:00:12:34:56 (10.0.0.2) <-- ./ukvm
    #        ||
    #        ||
    #        ||
    #      tap100 (10.0.0.1/24)        eth0
    #         \-------------------------|----
    #
    ip tuntap add tap100 mode tap
    ip link set tap100 up
    ip addr add 10.0.0.1/24 dev tap100
    sudo ip neigh add 10.0.0.2 lladdr ${MAC} nud permanent dev tap100
elif [[ "o$TARGET" == "ovirtio" ]]; then
    echo "configuring network for Solo5 virtio target"
    # remove old ukvm network setup
    ip link del tap100

    # virtio network setup
    brctl addbr virbr0
    ip link set virbr0 up
    ip link add veth1 type veth peer name veth2
    ip link set veth1 up
    ip link set veth2 up
    ip address add 10.0.0.1/24 dev veth1
    brctl addif virbr0 veth2
    ip neigh add 10.0.0.2 lladdr 52:54:00:12:34:56 nud permanent dev veth1
else
    echo "usage: sudo ./config_net.bash <ukvm|virtio>"
fi

# MYIP=`ip addr show eth0 |grep inet |grep eth0 |sed s/"^.*inet "//|sed s/"\/.*"//`
# iptables -t nat -A POSTROUTING ! -d 10.0.0.0/24 -o eth0 -j SNAT --to-source $MYIP
# iptables -A PREROUTING -t nat -i eth0 -p tcp --dport 80 -j DNAT --to 10.0.0.2:80
# iptables -A FORWARD -p tcp -d 10.0.0.2 --dport 80 -j ACCEPT






