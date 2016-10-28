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
boot). Check out our
[paper](https://www.usenix.org/system/files/conference/hotcloud16/hotcloud16_williams.pdf)
and
[presentation](https://www.usenix.org/sites/default/files/conference/protected-files/hotcloud16_slides_williams.pdf)
from USENIX HotCloud '16 for more information.

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

    opam switch --alias-of 4.03.0 mirage-solo5-dev

While Solo5 support has been merged into upstream MirageOS, this has not
yet been officially released. To use the latest development version of
MirageOS you will need to add the `mirage-dev` OPAM repository:

    opam repo add mirage-dev git://github.com/mirage/mirage-dev

Install the MirageOS frontend:

    opam depext -i mirage

Clone the MirageOS example applications repository (`mirage-dev` branch):

    git clone -b mirage-dev https://github.com/mirage/mirage-skeleton

Build the `stackv4` example:

    cd mirage-skeleton/stackv4
    mirage configure -t ukvm
    make

This will build the unikernel as `mir-stackv4.ukvm` (with a `ukvm-bin`
in the same directory) or `mir-stackv4.virtio`, depending on which
target you selected.

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

A specialized monitor called `ukvm-bin` is generated as part of the
build process, so the command line arguments may differ depending on
the needs of the unikernel.  To see the arguments, run `ukvm-bin` with
no arguments or `--help`.

To launch the unikernel:

    ./ukvm-bin --net=tap100 ./mir-stackv4.ukvm

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

If your hypervisor requires a full disk image to boot, you can use the
[solo5-mkimage](tools/mkimage/solo5-mkimage.sh) tool to build one. This tool is
automatically installed in your `$PATH` when using the `solo5-kernel-virtio`
OPAM package.

`solo5-mkimage` supports the following image formats:

* `raw`: A raw disk image, written out as a sparse file.
* `tar`: A disk image suitable for [uploading to](https://cloud.google.com/compute/docs/tutorials/building-images#publishingimage) Google Compute Engine.

The following devices are supported by the Solo5 `virtio` target:

* the serial console, fixed at COM1 and 115200 baud
* the KVM paravirtualized clock, if available
* a single virtio network device attached to the PCI bus
* a single virtio block device attached to the PCI bus

Note that Solo5 on virtio does not support ACPI power-off. This can manifest
itself in delays shutting down Solo5 guests running on hypervisors which wait
for the guest to respond to ACPI power-off before performing a hard shutdown.

# Running Mirage/Solo5 unikernels with Docker

[Unikernel runner](https://github.com/mato/docker-unikernel-runner)
integrates seamlessly with Docker networking and runs the unikernel and
`ukvm` in a container. Running the `stackv4` example is as simple as:

    docker run -ti --rm \
        --device=/dev/kvm:/dev/kvm \
        --device=/dev/net/tun:/dev/net/tun \
        --cap-add=NET_ADMIN mir-stackv4-ukvm

Refer to the
[documentation](https://github.com/mato/docker-unikernel-runner/blob/master/README.md)
for details.

# Developing Solo5 and ukvm

If you'd like to develop Solo5, ukvm and/or investigate porting other
unikernels to use Solo5 as a base layer, the public APIs are defined in
`kernel/solo5.h` and `ukvm/ukvm.h`. These interfaces are still evolving
and subject to change.

We also have some simple standalone unikernels written in C to test
Solo5, see `tests` for these.

The coding style for this project is "Linux with 4 spaces instead of
tabs".  The modified `checkpatch.pl` script helps give hints about
what violates this style.

The best place for Mirage-related discussions about Solo5 and/or
`ukvm` is to post to the [MirageOS-devel mailing list](http://lists.xenproject.org/cgi-bin/mailman/listinfo/mirageos-devel),
with general Solo5/`ukvm` discussions on <https://devel.unikernel.org/>.

# Debugging on ukvm

You can debug the unikernel running in ukvm using gdb, but need to
build a `ukvm-bin` with gdb support.  To do so, edit the Makefile
(generated in the case of Mirage), and add `gdb` to the `UKVM_MODULES`
make variable.  Then, running `make ukvm-bin` should build a version
with gdb support.

Start ukvm with the `--gdb` flag, like this:

    ./ukvm-bin mir-console.solo5-ukvm --gdb

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

# Acknowledgements

`ukvm` was originally written by Dan Williams and Ricardo Koller.  The
Solo5 kernel was originally written by Dan Williams.  The OPAM
packaging was done by Martin Lucina and Dan Williams.

This kernel got its start following the bare bones kernel tutorial at
<http://wiki.osdev.org/Bare_Bones>

Thanks to Daniel Bokser, who was an early code contributor who wrote
the timing code.
