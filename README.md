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

Trying out Solo5/Mirage on ukvm with Docker
-------------------------------------------

    docker pull djwillia/solo5-kernel-ukvm

Or, build it yourself from the provided Dockerfile.  

    cd docker
    docker build -t djwillia/solo5 .

Once you have the image, run it as follows:

    docker run -d --privileged --name solo5-kernel-ukvm -t djwillia/solo5-mirage-ukvm

We run the container as a privileged container because it will be
starting configuring the virtual network and starting VMs
(specifically Solo5/Mirage on ukvm).  A `docker ps` will show an
instance of the `solo5-kernel-ukvm` image running.  We can get a login
shell (as user/pass solo5/solo5) in the container using:

    docker exec -it solo5-kernel-ukvm /bin/bash -l

The mirage-skeleton directory with sample mirage applications has
already been checked out.  For more information about these
applications, check out the [Mirage tutorials](https://mirage.io/wiki/hello-world). To build and run the `hello world` application:

    cd ~/mirage-skeleton/console

Use the `mirage` tool to configure for Solo5, then build it:

    mirage configure --solo5
    make

You should see a file called `mir-console.solo5-ukvm`.  Run it on ukvm:

    sudo $(which ukvm) mir-console.solo5-ukvm /dev/null tap100

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

Acknowledgements
----------------

`ukvm` was originally written by Dan Williams and Ricardo Koller.  The
Solo5 kernel was originally written by Dan Williams.  The OPAM
packaging was done by Martin Lucina and Dan Williams.

This kernel got its start following the bare bones kernel tutorial at
<http://wiki.osdev.org/Bare_Bones>

Thanks to Daniel Bokser, who was an early code contributor who wrote
the timing code.
