                |      ___|  
      __|  _ \  |  _ \ __ \  
    \__ \ (   | | (   |  ) | 
    ____/\___/ _|\___/____/  

The Solo5/Mirage Unikernel
--------------------------

This repository holds the code for a native x86_64 kernel that, when
linked against an application becomes a unikernel.  The project goals
are to develop a small, understandable base from which research in the
unikernel space can be conducted.  We don't want to implement any
general purpose OS component (e.g., threads, address spaces, etc.)
without thinking about its value in a unikernel environment.

The `mirage` branch contains the base kernel and the necessary build
environment to link Mirage applications into an executable Solo5
unikernel.  Other repositories that are needed to run Mirage/Solo5
unikernels are:

    https://github.com/djwillia/mirage-platform
    https://github.com/djwillia/mirage
    https://github.com/djwillia/mirage-console
    https://github.com/djwillia/mirage-net-solo5
    https://github.com/djwillia/mirage-block-solo5
    https://github.com/djwillia/mirage-skeleton
    https://github.com/djwillia/mirage-www

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

On Ubuntu 15.04, you will need the following package dependencies for
the base kernel:

    sudo apt-get install -y build-essential xorriso bridge-utils kvm iputils-ping

The default scripts will make KVM/QEMU create an interface veth0 and
add it to a bridge named virbr0.  The base Solo5 kernel will have an
IP address of 10.0.0.2 and can be pinged from the host.

Configure the network by running:

    sudo ./config_net.bash

Note: when playing with the network on the bare kernel, you may need
to add an arp entry to the host because the kernel doesn't respond to
arps:

    sudo arp -s 10.0.0.2 52:54:00:12:34:56

To run Mirage unikernels on Solo5, you will need to install a number
of other dependencies, mostly for OCaml:

    sudo apt-get update
    sudo apt-get install -y software-properties-common
    sudo add-apt-repository -y ppa:avsm/ppa
    sudo apt-get update
    sudo apt-get install -y ocaml ocaml-native-compilers camlp4-extra opam m4 pkg-config libpcre3-dev libxen-dev ncurses-dev time iptables

Then, configure OPAM as follows, and install packages from OPAM:

    opam init -a
    opam switch 4.01.0
    echo "eval \`opam config env\`" >> ~/.profile
    eval `opam config env`
    opam install -y oasis depext

Next, we clone more repositories containing the Mirage platform
bindings and device drivers that make Mirage applications work with
Solo5:

    mkdir ~/solo5-mirage
    cd ~/solo5-mirage
    for d in mirage mirage-block-solo5 mirage-console mirage-net-solo5 mirage-platform mirage-skeleton mirage-www; do
        git clone --branch=solo5 https://github.com/djwillia/$d.git;
    done

Now we are ready to configure a unikernel to run a Mirage application.
Return to the directory containing the base Solo5 kernel (on the
`mirage` branch). Edit the `mirage.config` file to specify where you
cloned all of the extra repositories.  Then, configure the system to
build one of the supported Mirage applications (i.e., execute ONE of
the following lines):

    make config_console
    make config_block
    make config_kv_ro_crunch
    make config_kv_ro
    make config_stackv4
    make config_www

To just build the kernel.iso, which can then be loaded into
KVM/QEMU or VirtualBox, type

    make 

Or, to build the unikernel and run in KVM/QEMU, type

    make kvm

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

Acknowledgements
----------------

This kernel got its start following the bare bones kernel tutorial at
<http://wiki.osdev.org/Bare_Bones>

Thanks to Daniel Bokser, who was an early code contributor who wrote
the timing code.
