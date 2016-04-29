#!/bin/bash

#
# 52:54:00:12:34:56 (10.0.0.2) <-- ./ukvm
#        ||
#        ||
#        ||
#      tap100 (10.0.0.1/24)        eth0
#         \-------------------------|----
#

MAC=52:54:00:12:34:56

ip tuntap add tap100 mode tap
ip link set tap100 up
ip addr add 10.0.0.1/24 dev tap100
sudo ip neigh add 10.0.0.2 lladdr ${MAC} nud permanent dev tap100

# MYIP=`ip addr show eth0 |grep inet |grep eth0 |sed s/"^.*inet "//|sed s/"\/.*"//`
# iptables -t nat -A POSTROUTING ! -d 1.0.0.0/24 -o eth0 -j SNAT --to-source $MYIP
# iptables -A PREROUTING -t nat -i eth0 -p tcp --dport 80 -j DNAT --to 1.0.0.2:80
# iptables -A FORWARD -p tcp -d 1.0.0.2 --dport 80 -j ACCEPT


