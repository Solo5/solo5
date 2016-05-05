                |      ___|  
      __|  _ \  |  _ \ __ \  
    \__ \ (   | | (   |  ) | 
    ____/\___/ _|\___/____/  

The Solo5 Unikernel
-------------------

The Solo5 unikernel is most useful to run MirageOS, either on KVM/QEMU
or on a specialized "unikernel monitor" called `ukvm`.  

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

Trying out Solo5/Mirage on ukvm or KVM/QEMU with Docker
-------------------------------------------------------

    docker pull djwillia/solo5-mirage

Or, build it yourself from the provided Dockerfile.  

    cd docker
    docker build -t djwillia/solo5-mirage .

Once you have the image, run it as follows:

    docker run -d --privileged --name solo5-mirage -t djwillia/solo5-mirage

We run the container as a privileged container because it will be
starting configuring the virtual network and starting VMs
(specifically Solo5/Mirage unikernels).  A `docker ps` will show an
instance of the `solo5-mirage` image running.  We can get a login
shell in the container using:

    docker exec -it solo5-mirage /bin/bash -l

The mirage-skeleton directory with sample mirage applications has
already been checked out.  For more information about these
applications, check out the [Mirage tutorials](https://mirage.io/wiki/hello-world). To build and run the `hello world` application:

    cd ~/mirage-skeleton/console

First we will describe how to build and run unikernels on ukvm, but
the process for QEMU is similar.

### Running on ukvm

Use the `mirage` tool to configure for ukvm, 

    mirage configure --ukvm

then build it:

    make

You should see a file called `mir-console.solo5-ukvm`. To run the
unikernel on ukvm:

    sudo $(which ukvm) mir-console.solo5-ukvm ~/disk.img tap100

You should see something like this:

    Providing disk: 0 sectors @ 512 = 0 bytes
                |      ___|
      __|  _ \  |  _ \ __ \
    \__ \ (   | | (   |  ) |
    ____/\___/ _|\___/____/
    Solo5: new bindings
    hello
    world
    hello
    world
    hello
    world
    hello
    world
    Kernel done.
    Goodbye!
    KVM_EXIT_HLT

To exit ukvm, type `C-c` in ukvm.

### Running on KVM/QEMU

For KVM/QEMU, the steps are similar except the file produced is named
`mir-console.iso` and the command to invokes KVM/QEMU:

    mirage configure --qemu
    make
    sudo kvm -s -nographic -boot d -cdrom mir-console.iso -device virtio-net,netdev=n0 -netdev tap,id=n0,ifname=tap100,script=no,downscript=no -drive file=~/disk.img,format=raw,if=virtio

To exit QEMU, type `C-a x`.

Debugging
---------

You can debug the unikernel running in ukvm using gdb. Start
ukvm like this:

    make gdb

And then from another console start gdb and connect to the remote target
listening at localhost:1234:

    (gdb) target remote localhost:1234

Here is a typical gdb session:

    (gdb) target remote localhost:1234
    Remote debugging using localhost:1234
    kernel_main (size=536870912, kernel_end=2625536) at kernel.c:36
    36    void kernel_main(uint64_t size, uint64_t kernel_end) {
    (gdb) break unikernel.ml:9
    Breakpoint 11 at 0x104c39: file unikernel.ml, line 9.
    (gdb) c
    Continuing.

    Breakpoint 11, camlUnikernel__loop_1401 () at unikernel.ml:9
    9            C.log c "hello";
    (gdb) bt
    #0  camlUnikernel__loop_1401 () at unikernel.ml:9
    #1  0x000000000010480e in camlMain__fun_1487 () at main.ml:32
    #2  0x0000000000178ad9 in camlCamlinternalLazy__force_lazy_block_1010 () at camlinternalLazy.ml:25
    #3  0x00000000001049e4 in camlMain__fun_1494 () at main.ml:38
    #4  0x0000000000178ad9 in camlCamlinternalLazy__force_lazy_block_1010 () at camlinternalLazy.ml:25
    #5  0x0000000000104bfa in camlMain__entry () at src/core/lwt.ml:660
    #6  0x0000000000100ed9 in caml_program ()
    #7  0x00000000001c26a6 in caml_start_program ()
    #8  0x0000000000000000 in ?? ()
    (gdb) s

    Breakpoint 11, camlUnikernel__loop_1401 () at unikernel.ml:9
    9            C.log c "hello";

Acknowledgements
----------------

`ukvm` was originally written by Dan Williams and Ricardo Koller.  The
Solo5 kernel was originally written by Dan Williams.  The OPAM
packaging was done by Martin Lucina and Dan Williams.

This kernel got its start following the bare bones kernel tutorial at
<http://wiki.osdev.org/Bare_Bones>

Thanks to Daniel Bokser, who was an early code contributor who wrote
the timing code.
