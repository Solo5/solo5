# Building Solo5 and running Solo5-based unikernels

As Solo5 is essentially a piece of "middleware" interfacing unikernel-style
applications with their host systems, it is not an end-developer product as
such.

To get started as a developer with Solo5, please refer primarily to the
instructions provided by unikernel project you intend to develop applications
with:

- MirageOS: https://mirage.io/wiki/install
- IncludeOS: http://www.includeos.org/get-started.html

That said, this document provides general information -- not specific to any
unikernel -- about building Solo5, and running Solo5-based unikernels.

## Building Solo5

Solo5 itself has the following build dependencies:

* a 64-bit Linux, FreeBSD or OpenBSD system (see also below under "Supported
  targets" for further requirements),
* a C99 compiler; recent versions of GCC and clang are supported,
* GNU make,
* full host system headers (on Linux, kernel headers are not always installed
  by default).

To build Solo5 from the Git repository, it is sufficient to run `make` (`gmake`
on BSD systems). This will build all _targets_ supported on the host system.

To run the built-in self-tests:

    tests/setup-tests.sh # As root, sets up `tap100` for networking
    tests/run-tests.sh   # Root required to run network tests

Note that Solo5 does not support cross-compilation; with the exception of the
_muen_ target (which is not self-hosting), you should build Solo5 and
unikernels on a system matching the host you will be running them on.

## Supported targets

Supported _targets_, host operating systems/hypervisors and processor
architectures:

Production:

* _ukvm_: Linux/KVM, using ukvm as a monitor, on the x86\_64 architecture.
* _ukvm_; FreeBSD vmm, using ukvm as a monitor, on the x86\_64 architecture.
  FreeBSD 11-RELEASE or later is required.

Experimental:

* _ukvm_: Linux/KVM, using ukvm as a monitor, on the aarch64 architecture. You
  will need hardware capable of running a recent (v4.14+) 64-bit mainline
  kernel and a 64-bit Linux distribution.
* _ukvm_: OpenBSD vmm, using ukvm as a monitor, on the x86\_64 architecture.
  OpenBSD 6.4 or later is required.
* _muen_: The Muen Separation Kernel, on the x86\_64 architecture. Please see
  this [article](https://muen.sk/articles.html#mirageos-unikernels) for
  Muen-specific instructions.

Limited:

* _virtio_: Any hypervisor which virtualizes an x86\_64 system with virtio
  devices (e.g.  Linux/KVM with QEMU as a monitor, Google Compute Engine). See
  below under "_virtio_: Limitations" for details.

# Running Solo5-based unikernels

Solo5 itself does not provide a high-level mangement or orchestration layer for
unikernels -- this is intended to be provided by separate downstream projects.
If you are coming from Linux containers you can think of Solo5 as conceptually
occupying the same space in the stack as, for example, `runc`.

If you are looking for a high-level stack for deploying unikernels, one such
project is [Albatross](https://hannes.nqsb.io/Posts/VMM).

The following examples use the standalone
[test\_ping\_serve](tests/test_ping_serve/test_ping_serve.c) unikernel which is
built as part of the normal Solo5 build process.

The products of building a Solo5 unikernel are, depending on the _target_, one
or two artifacts:

- an ELF binary containing the built unikernel,
- for the _ukvm_ target, a specialized _[monitor](architecture.md)_, by
  convention currently built as `ukvm-bin` alongside the unikernel binary.

`test_ping_serve` is a minimalist network test which will respond only to ARP
and ICMP echo requests sent to the hard-coded IP address of `10.0.0.2`. It
accepts two possible command line arguments: Either `verbose` for verbose
operation or `limit` to terminate after having sent 100,000 ICMP echo replies.

## Setting up

By convention, we will use the `tap100` interface to talk to the unikernel.

To set up the `tap100` interface on Linux, run (as root):

    ip tuntap add tap100 mode tap
    ip addr add 10.0.0.1/24 dev tap100
    ip link set dev tap100 up

To set up vmm and the `tap100` interface on FreeBSD, run (as root):

    kldload vmm
    kldload if_tap
    sysctl -w net.link.tap.up_on_open=1
    ifconfig tap100 create 10.0.0.1/24 link0

To set up vmm and the `tap100` interface on OpenBSD, run (as root):

    cd /dev
    ./MAKEDEV tap100
    ifconfig tap100 inet 10.0.0.1 netmask 255.255.255.0

## _ukvm_: Running on Linux, FreeBSD and OpenBSD

A specialized monitor called `ukvm-bin` is generated as part of the
build process, so the command line arguments may differ depending on
the needs of the unikernel.  To see the arguments, run `ukvm-bin` with
no arguments or `--help`.

On Linux, `ukvm-bin` only requires access to `/dev/kvm` and `/dev/net/tun`, and
thus does NOT need to run as `root` provided your have granted the user in
question the correct permissions. On FreeBSD and OpenBSD, `root` privileges are
currently required in order to access the `vmm` APIs.

To launch the unikernel:

    ./ukvm-bin --net=tap100 -- test_ping_serve.ukvm verbose

Use `^C` to terminate the unikernel.

## _virtio_: Running with KVM/QEMU on Linux, or bhyve on FreeBSD

The [solo5-run-virtio](tools/run/solo5-run-virtio.sh) script provides a wrapper
to correctly launch `qemu-system-x86_64` or `bhyve` on the host system.  Using
it is not required; by default it will print the commands used to setup and
launch the guest VM. You can run these manually if desired.

To launch the unikernel:

    solo5-run-virtio -n tap100 -- test_ping_serve.virtio verbose

Use `^C` to terminate the unikernel.

## _virtio_: Running on other hypervisors

The _virtio_ target produces a unikernel that uses the multiboot
protocol for booting. If your hypervisor can boot a multiboot-compliant
kernel directly then this is the preferred method.

If your hypervisor requires a full disk image to boot, you can use the
[solo5-mkimage](tools/mkimage/solo5-mkimage.sh) tool to build one.

`solo5-mkimage` supports the following image formats:

* `raw`: A raw disk image, written out as a sparse file.
* `tar`: A disk image suitable for [uploading to](https://cloud.google.com/compute/docs/tutorials/building-images#publishingimage) Google Compute Engine.

## _virtio_: Limitations

The _virtio_ target was the initial target supported by Solo5 -- while we are
keeping it around as it is useful to some users, it is essentially a
"compatibility layer" providing the Solo5 interface atop "legacy" full-system
virtualization. As the goals of Solo5 have since evolved, we do not expect to
devote a substantial amount of time to the further development of _virtio_.

Therefore, we recommend that new deployments use the _ukvm_ target instead.

The following devices are supported by the _virtio_ target:

* the serial console, fixed at COM1 and 115200 baud
* the KVM paravirtualized clock, if available
* a single virtio network device attached to the PCI bus
* a single virtio block device attached to the PCI bus

Note that _virtio_ does not support ACPI power-off. This can manifest itself in
delays shutting down Solo5 guests running on hypervisors which wait for the
guest to respond to ACPI power-off before performing a hard shutdown.

----

Next: [Debugging Solo5-based unikernels](debugging.md)
