                |      ___|  
      __|  _ \  |  _ \ __ \  
    \__ \ (   | | (   |  ) | 
    ____/\___/ _|\___/____/  

# Status update, February 2018

Solo5 is currently undergoing major refactoring including breaking API/ABI
changes. This document reflects the state of the project as of the last formal
release (version 0.2.2), and will be updated once this refactoring has
concluded.

If you are a Mirage/Solo5 user and would like to keep using Mirage 3.0 based on
Solo5 version 0.2.x, the instructions here are still applicable and you do not
need to do anything special.

If you want to follow the development version you should use the OPAM remote in
the [Solo5/opam-solo5](https://github.com/Solo5/opam-solo5) repository and
update your packages regularly. For more details, refer to this mirageos-devel
[thread](https://lists.xenproject.org/archives/html/mirageos-devel/2018-02/msg00011.html).

# About Solo5

Solo5 is most useful as a "base layer" to run
[MirageOS](https://mirage.io/) and [IncludeOS](http://www.includeos.org/) unikernels, either on various existing
hypervisors (KVM/QEMU, bhyve) or on a specialized "unikernel monitor" called
`ukvm`.

# Quickstart

You can run a hello-world application like this:

    $ make
    $ ./tests/test_hello/ukvm-bin ./tests/test_hello/test_hello.ukvm

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

# Building MirageOS unikernels with Solo5

Support for Solo5 as a target is integrated since the release of MirageOS 3.0,
which adds two new targets to the `mirage configure` command:

1. `ukvm`: A specialized "unikernel monitor" which runs on Linux
   (`x86_64` and `arm64`) and uses KVM directly via `/dev/kvm`.
2. `virtio`: An `x86_64` system with `virtio` network and disk
   interfaces. Use this target for QEMU/KVM, plain QEMU, bhyve or other
   hypervisors (see below).

You can build for either of these targets using the standard MirageOS build
process, which will output either a `.ukvm` or `.virtio` unikernel binary.

For the `ukvm` target, in addition to the `.ukvm` unikernel binary a `ukvm-bin`
binary will be built. This is the `ukvm` monitor specialised for your
unikernel.

# Running Solo5 unikernels directly

The following examples use the standalone
[test_ping_serve](tests/test_ping_serve/test_ping_serve.c) unikernel which is
built as part of the normal Solo5 build process. 

`test_ping_serve` is a minimalist network test which will respond only to ARP
and ICMP echo requests sent to the hard-coded IP address of `10.0.0.2`. It
accepts two possible command line arguments: Either `verbose` for verbose
operation or `limit` to terminate after having sent 100,000 ICMP echo replies.

**Setting up**

By convention, we will use the `tap100` interface to talk to the unikernel.

To set up the `tap100` interface on Linux, run (as root):

    ip tuntap add tap100 mode tap
    ip addr add 10.0.0.1/24 dev tap100
    ip link set dev tap100 up

To set up bhyve and the `tap100` interface on FreeBSD, run (as root):

    kldload vmm
    kldload if_tap
    kldload nmdm
    sysctl -w net.link.tap.up_on_open=1
    ifconfig tap100 create 10.0.0.1/24 link0 up

**Running with ukvm on Linux**

A specialized monitor called `ukvm-bin` is generated as part of the
build process, so the command line arguments may differ depending on
the needs of the unikernel.  To see the arguments, run `ukvm-bin` with
no arguments or `--help`.

To launch the unikernel:

    ./ukvm-bin --net=tap100 ./test_ping_serve.ukvm verbose

Use `^C` to terminate the unikernel.

**Running with KVM/QEMU on Linux, or bhyve on FreeBSD**

To launch the unikernel:

    solo5-run-virtio -n tap100 ./test_ping_serve.virtio verbose

Use `^C` to terminate the unikernel.

The [solo5-run-virtio](tools/run/solo5-run-virtio.sh) script is automatically
installed in your `$PATH` when using the `solo5-kernel-virtio` OPAM package as
part of a MirageOS build environment.
Using it is not required; by default it will print the commands used to setup
and launch the guest VM. You can run these manually if desired.

**Running virtio unikernels with other hypervisors**

The `virtio` target produces a unikernel that uses the multiboot
protocol for booting. If your hypervisor can boot a multiboot-compliant
kernel directly then this is the preferred method.

If your hypervisor requires a full disk image to boot, you can use the
[solo5-mkimage](tools/mkimage/solo5-mkimage.sh) tool to build one. This tool is
automatically installed in your `$PATH` when using the `solo5-kernel-virtio`
OPAM package as part of a MirageOS build environment.

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

# Developing Solo5 and ukvm

If you'd like to develop Solo5, ukvm and/or investigate porting other
unikernels to use Solo5 as a base layer, the public APIs are defined in
`kernel/solo5.h` and `ukvm/ukvm.h`. These interfaces are still evolving
and subject to change.

We also have some simple standalone unikernels written in C to test
Solo5, see `tests` for these.

The coding style for this project is "as for the Linux kernel, but with 4
spaces instead of tabs".

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

    $ cd tests/test_hello
    $ ./ukvm-bin --gdb test_hello.ukvm

And then from another console start gdb and connect to the remote target
listening at `localhost:1234`:

    $ gdb --ex="target remote localhost:1234" test_hello.ukvm

Here is a typical gdb session:

    (gdb) break puts
    Breakpoint 1 at 0x100530: puts. (2 locations)
    (gdb) c
    Continuing.

    Breakpoint 1, puts (s=s@entry=0x107808 "\n**** Solo5 standalone test_hello ****\n\n") at test_hello.c:25
    25	{
    (gdb) bt
    #0  puts (s=s@entry=0x107808 "\n**** Solo5 standalone test_hello ****\n\n") at test_hello.c:25
    #1  0x0000000000106a8e in solo5_app_main (cmdline=cmdline@entry=0x6000 "") at test_hello.c:31
    #2  0x0000000000100089 in _start (arg=0x5000) at ukvm/kernel.c:42
    (gdb) s
    26	    solo5_console_write(s, strlen(s));
    (gdb) 
