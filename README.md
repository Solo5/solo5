                |      ___|  
      __|  _ \  |  _ \ __ \  
    \__ \ (   | | (   |  ) | 
    ____/\___/ _|\___/____/  

The Solo5 Unikernel
-------------------

This repository holds the code for a native x86_64 kernel that, when
linked against an application becomes a unikernel.  The project goals
are to develop a small, understandable base from which research in the
unikernel space can be conducted.  We don't want to implement any
general purpose OS component (e.g., threads, address spaces, etc.)
without thinking about its value in a unikernel environment.

The `master` branch contains the base kernel.  Other branches include
application code that is driving the development of the kernel, but
should stay separate.

The kernel should run on any x86_64 hypervisor that uses virtio
devices.  At this point, we have run it on QEMU/KVM and VirtualBox.
Output is through the serial port.

Building and running
--------------------

You must have two cross compilers capable of building x86_64-elf
targets as well as i686-elf targets.  In particular, your `$PATH`
should include `x86_64-elf-as`, `x86_64-elf-gcc`, `x86_64-elf-gcc`,
`i686-elf-as`, `i686-elf-gcc`, and `i686-elf-gcc`.  For example, if
you have cross compilers in `~/opt/cross/bin`, do

    PATH=~/opt/cross/bin/:$PATH

For instructions on building cross compilers, see
<http://wiki.osdev.org/GCC_Cross-Compiler>

On Ubuntu 15.04, you will need the following package dependencies:

    sudo apt-get install -y build-essential xorriso bridge-utils kvm iputils-ping

To just build the kernel.iso, which can then be loaded into
KVM/QEMU or VirtualBox, type

    make 

Setting up KVM/QEMU for Solo5
-----------------------------

For network tests, up to this point, we have the kernel pinging on a
local bridge.

The default scripts will make KVM/QEMU create an interface veth0 and
add it to a bridge named virbr0.  The kernel will have an IP address
of 10.0.0.2 and can be pinged from the host.

Configure the network by running:

    ./config.bash

Then, to build the kernel and run in KVM/QEMU, type

    make kvm

Note: when playing with the network on the bare kernel, you may need
to add an arp entry to the host because the kernel doesn't respond to
arps

    sudo arp -s 10.0.0.2 52:54:00:12:34:56

Debugging
---------

The most convenient way to debug is using QEMU with GDB.  This is
tricky because gdb doesn't like the mode switching between the 32-bit
loader and the 64-bit kernel.  So we basically need to get the kernel
to a point where we can attach where the kernel is already in 64-bit
mode.  My trick is to have an infinite loop that we break by setting a
volatile int variable (see kernel.c).

Start qemu with -s:

    qemu-system-x86_64 -s -nographic -cdrom kernel.iso

*or*

    make qemu

*or*

    make kvm

In another terminal, start gdb:

    gdb kernel/kernel
    target remote localhost:1234
    set gdb=1
    c

Or, I have that in a command file:

    gdb kernel/kernel --command gdb.txt

If the kernel crashed in an interrupt handler, you can find out where
the interrupt came from in gdb:

    x/1x interrupt_stack_page+4040

References
----------

This kernel got its start following the bare bones kernel tutorial at
<http://wiki.osdev.org/Bare_Bones>

Thanks to Daniel Bokser, who was an early code contributor who wrote
the timing code.