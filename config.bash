#!/bin/bash

# Copyright (c) 2015, IBM
# Author(s): Dan Williams <djwillia@us.ibm.com>
#
# Permission to use, copy, modify, and/or distribute this software for
# any purpose with or without fee is hereby granted, provided that the
# above copyright notice and this permission notice appear in all
# copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
# DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
# OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

REQ_PROGS=(i686-elf-as i686-elf-gcc x86_64-elf-as x86_64-elf-gcc)

for prog in ${REQ_PROGS[@]};do
    if ! which $prog > /dev/null;then
        echo "Program $prog not found in PATH."
        echo "To build the required cross compilers for both x86_64 and i686 architectures, please follow the tutorial on:"
        echo "http://wiki.osdev.org/GCC_Cross-Compiler "
        exit 1
    fi
done

echo "Configuring network"
sudo brctl addbr virbr0
sudo ip link set virbr0 up
sudo ip link add veth1 type veth peer name veth2
sudo ip link set veth1 up
sudo ip link set veth2 up
sudo ip address add 10.0.0.1/24 dev veth1
sudo brctl addif virbr0 veth2
sudo ip neigh add 10.0.0.2 lladdr 52:54:00:12:34:56 nud permanent dev veth1

echo -e "Next, try\n\tmake kvm"
