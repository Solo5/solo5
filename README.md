                |      ___|  
      __|  _ \  |  _ \ __ \  
    \__ \ (   | | (   |  ) | 
    ____/\___/ _|\___/____/  

The Solo5/Mirage Unikernel on ukvm
----------------------------------

The `ukvm-mirage` branch contains the base kernel and the necessary
build environment to link Mirage applications into an executable Solo5
unikernel.  Rather than producing a target that runs on KVM/QEMU like
the other branches, there is a specialized "unikernel monitor" called
`ukvm` that is also compiled in the tree.

`ukvm` runs as a Linux process and uses KVM.  The goal of `ukvm` is to
be a small, modular monitor, in which its functionality and interfaces
are customized to the unikernel that is being built.  In other words,
the unikernel monitor exhibits the characteristic of "only what is
needed" for the unikernel to run.  It has the potential to provide a
thinner interface to the guest unikernel (thinner than either a
container or a VM), a simpler I/O interface (e.g., packet send rather
than virtio), and better performance due to its simplicity (e.g., fast
boot).

So far, we have a small monitor implementation that is not yet
modular, but demonstrates some dramatically simple interfaces and a
fast boot time.

Other repositories that are needed to run Mirage/Solo5 unikernels on
ukvm are:

    https://github.com/djwillia/mirage-platform
    https://github.com/djwillia/mirage
    https://github.com/djwillia/mirage-console
    https://github.com/djwillia/mirage-net-solo5
    https://github.com/djwillia/mirage-block-solo5
    https://github.com/djwillia/mirage-skeleton
    https://github.com/djwillia/mirage-bootvar-solo5
    https://github.com/djwillia/mirage-entropy
    https://github.com/djwillia/ocaml-nocrypto 

Trying out Solo5/Mirage on ukvm with Docker
-------------------------------------------

We have packaged the Solo5/Mirage on ukvm development environment with
Docker, so now it's easy to try out Solo5/Mirage on ukvm !  First, get
the container from Docker Hub:

    docker pull djwillia/solo5-mirage-ukvm

Or, build it yourself from the provided Dockerfile.  

    cd docker
    docker build -t djwillia/solo5-mirage-ukvm .

Once you have the image, run it as follows:

    docker run -d --privileged --name solo5-mirage-ukvm -t djwillia/solo5-mirage-ukvm

We run the container as a privileged container because it will be
starting configuring the virtual network and starting VMs
(specifically Solo5/Mirage on ukvm).  A `docker ps` will show an
instance of the `solo5-mirage-ukvm` image running.  We can get a login
shell (as user/pass solo5/solo5) in the container using:

    docker exec -it solo5-mirage-ukvm /bin/bash -l

You can now build and run several Mirage applications as Solo5
unikernels on ukvm.  First enter the Solo5 directory:

    cd ~/solo5
    
Then, configure the system to build one of the supported Mirage
applications (i.e., execute ONE of the following lines):

    make config_console
    make config_block
    make config_kv_ro_crunch
    make config_kv_ro
    make config_stackv4
    make config_static_web

For more information about these applications, check out the [Mirage
tutorials](https://mirage.io/wiki/hello-world). Run the application
with:

    make run

When you are satisfied with the Mirage application, exit ukvm running
Solo5 in the terminal with `C-c`.  Even if you exit these terminals
(`C-d`), the container will continue to run until you kill it (in the
host) with:

    sudo docker kill solo5-mirage-ukvm


Building and running yourself
-----------------------------

This is a bit harder than using the Dockerfile, because of building
cross compilers for Solo5 and keeping track of other repositories
containing Mirage bindings and drivers for Solo5.

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

The base Solo5 kernel will have an IP address of 10.0.0.2 and can be
pinged from the host.

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
    opam switch 4.02.3
    echo "eval \`opam config env\`" >> ~/.profile
    eval `opam config env`
    opam install -y oasis depext

Next, we clone more repositories containing the Mirage platform
bindings and device drivers that make Mirage applications work with
Solo5:

    mkdir ~/solo5-mirage
    cd ~/solo5-mirage
    for d in mirage mirage-block-solo5 mirage-console mirage-net-solo5 mirage-platform mirage-skeleton mirage-www mirage-entropy mirage-bootvar-solo5 ocaml-nocrypto; do
        git clone https://github.com/djwillia/$d.git;
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
    make config_static_web

To build the unikernel and run in ukvm, type

    make run

Acknowledgements
----------------

`ukvm` was written by Dan Williams and Ricardo Koller.  The Solo5
kernel was written by Dan Williams.

This kernel got its start following the bare bones kernel tutorial at
<http://wiki.osdev.org/Bare_Bones>

Thanks to Daniel Bokser, who was an early code contributor who wrote
the timing code.
