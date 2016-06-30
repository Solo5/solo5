                |      ___|  
      __|  _ \  |  _ \ __ \  
    \__ \ (   | | (   |  ) | 
    ____/\___/ _|\___/____/  

# About Solo5

Solo5 is most useful as a "base layer" to run
[MirageOS](https://mirage.io/) unikernels, either on KVM/QEMU or on a
specialized "unikernel monitor" called `ukvm`.

# About ukvm

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

# Building Mirage/Solo5 unikernels

These instructions assume that you have an OCaml (4.02.3 or newer)
development environment with the OPAM package manager installed. If you
are developing with Docker, you can use the one of the pre-built
`ocaml/opam` [images](https://hub.docker.com/r/ocaml/opam/).

Mirage/Solo5 supports two different targets to `mirage configure`:

1. `ukvm`: A specialized "unikernel monitor" which runs on Linux
   (`x86_64`) and uses KVM directly via `/dev/kvm`.
2. `virtio`: An `x86_64` system with `virtio` network and disk
   interfaces. Use this target for QEMU/KVM, plain QEMU or other
   hypervisors (see below).

Start with a fresh OPAM switch for the Mirage/Solo5 target you will be
using: (If you are using the pre-built Docker images you can skip this
step.)

    opam switch --alias-of 4.03.0 mirage-solo5-ukvm

Add the OPAM repository for Mirage/Solo5:

    opam repo add solo5 git://github.com/djwillia/opam-solo5

If you will be using the `ukvm` target, ensure that a "Linux kernel
headers" package is installed. For Debian-based distributions this is
`linux-libc-dev`, for Alpine Linux this is `linux-headers`.

Install Mirage/Solo5:

    opam depext -i mirage

Clone the MirageOS example applications repository:

    git clone https://github.com/mirage/mirage-skeleton

Build the `stackv4` example:

    cd mirage-skeleton/stackv4
    mirage configure --ukvm
    make

This will build the unikernel as `mir-stackv4.ukvm` or
`mir-stackv4.virtio`, depending on which target you selected.

# Running Mirage/Solo5 unikernels directly

These examples show how to run Mirage/Solo5 unikernels directly.  If
you'd like to use Docker instead, skip to the next section.

**Setting up networking for ukvm or qemu on Linux**

The following examples assume you have successfully built the MirageOS
`stackv4` application, and use the default networking configuration for
the unikernel (a static IP address of `10.0.0.2/24`). Run the `stackv4`
unikernel with an argument of `--help=plain` to see the options for
modifying this.

Before using either ukvm or qemu, set up the `tap100` interface we will
use to talk to the unikernel:

    ip tuntap add tap100 mode tap
    ip addr add 10.0.0.1/24 dev tap100
    ip link set dev tap100 up

**Running with ukvm on Linux**

To launch the unikernel:

    ukvm /dev/zero tap100 ./mir-stackv4.ukvm

Use `^C` to terminate the unikernel.

**Running with qemu on Linux**

To launch the unikernel:

    qemu-system-x86_64 -nographic -vga none -kernel ./mir-stackv4.virtio \
        -device virtio-net,netdev=n0 \
        -netdev tap,id=n0,ifname=tap100,script=no,downscript=no

Use `^A x` to terminate the unikernel.

**Running virtio unikernels with other hypervisors**

The `virtio` target produces a unikernel that uses the multiboot
protocol for booting. If your hypervisor can boot a multiboot-compliant
kernel directly then this is the preferred method.

If your hypervisor requires a full "disk image" to boot, look at the
`solo5-build-iso.bash` script for an example of how to embed GRUB and
the unikernel into an ISO image.

# Running Mirage/Solo5 unikernels with Docker

Use
[docker-unikernel-runner](https://github.com/mato/docker-unikernel-runner),
which integrates seamlessly with Docker networking and runs the
unikernel and hypervisor in a container.

# Debugging on ukvm

You can debug the unikernel running in ukvm using gdb. Start
ukvm with the `--gdb` flag, like this:

    sudo $(which ukvm) ~/disk.img tap100 mir-console.solo5-ukvm --gdb

And then from another console start gdb and connect to the remote target
listening at `localhost:1234`:

    $ gdb mir-console.solo5-ukvm

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

# Developing Solo5 or ukvm

If you'd like to develop Solo5, ukvm and/or investigate porting other
unikernels to use Solo5 as a base layer, the public APIs are defined in
`kernel/solo5.h` and `ukvm/ukvm.h`. These interfaces are still evolving
and subject to change.

We also have some simple standalone unikernels written in C to test
Solo5, see `kernel/test_*` for these.

# Acknowledgements

`ukvm` was originally written by Dan Williams and Ricardo Koller.  The
Solo5 kernel was originally written by Dan Williams.  The OPAM
packaging was done by Martin Lucina and Dan Williams.

This kernel got its start following the bare bones kernel tutorial at
<http://wiki.osdev.org/Bare_Bones>

Thanks to Daniel Bokser, who was an early code contributor who wrote
the timing code.
