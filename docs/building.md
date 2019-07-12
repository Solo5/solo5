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
_muen_ and _genode_ targets (which are not self-hosting), you should build
Solo5 and unikernels on a system matching the host you will be running them on.

## Supported targets

Supported _targets_, host operating systems/hypervisors and processor
architectures:

Production:

* _hvt_: Linux/KVM, using `solo5-hvt` as a tender, on the x86\_64 architecture.
* _hvt_; FreeBSD vmm, using `solo5-hvt` as a tender, on the x86\_64
  architecture.  FreeBSD 11-RELEASE or later is required.

Experimental:

* _hvt_: Linux/KVM, using `solo5-hvt` as a tender, on the aarch64 architecture.
  You will need hardware capable of running a recent (v4.14+) 64-bit mainline
  kernel and a 64-bit Linux distribution.
* _hvt_: OpenBSD vmm, using `solo5-hvt` as a tender, on the x86\_64
  architecture.  OpenBSD 6.4 or later is required.
* _spt_: Linux systems on the x86\_64 and aarch64 architectures, using
  `solo5-spt` as a tender.
* _muen_: The Muen Separation Kernel, on the x86\_64 architecture. Please see
  this [article](https://muen.sk/articles.html#mirageos-unikernels) for
  Muen-specific instructions.
* _genode_: The Genode Operating System Framework on the x86\_64 architecture.
  Unikernels may be linked against a shared library containing Solo5 bindings
  and executed as a native Genode component. The bindings library is provided
  here in the form of a stub library that can be built using a stock C
  toolchain along with the C++ implementation that must be built using the
  Genode toolchain.

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
[test\_net](../tests/test_net/test_net.c) unikernel which is
built as part of the normal Solo5 build process.

~~The products of building a Solo5 unikernel are, depending on the _target_, one
or two artifacts: (...)~~

_TODO: Tie in / link to application manifest documentation?._

`test_net` is a minimalist network test which will respond only to ARP
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

## _hvt_: Running on Linux, FreeBSD and OpenBSD with hardware virtualization

The _hvt_ ("hardware virtualized tender") target supports Linux, FreeBSD and
OpenBSD systems and uses hardware virtualization to isolate the guest
unikernel.

On Linux, the `solo5-hvt` _tender_ only requires access to `/dev/kvm` and
`/dev/net/tun`, and thus does NOT need to run as `root` provided your have
granted the user in question the correct permissions.

On FreeBSD and OpenBSD, `root` privileges are currently required in order to
access the `vmm` APIs. However, before starting the unikernel, the _tender_
will drop privileges to an unprivileged user and, in the case of OpenBSD, use
`pledge(2)` to further lower its privileges.

To launch the unikernel:

    ../tenders/hvt/solo5-hvt --net:service=tap100 -- test_net.hvt verbose

Use `^C` to terminate the unikernel.

## _spt_: Running on Linux with a strict seccomp sandbox

The _spt_ ("sandboxed process tender") target currently supports Linux systems
only, and uses a _strict_ (minimal whitelist) seccomp sandbox to isolate the
guest unikernel, which runs as a user process on the host.

The `solo5-spt` tender is built as part of Solo5, and does not require any
special privileges to run.

To launch the unikernel:

    ../tenders/spt/solo5-spt --net:service=tap100 -- test_net.spt verbose

## _virtio_: Running with KVM/QEMU on Linux, or bhyve on FreeBSD

The [solo5-virtio-run](../scripts/virtio-run/solo5-virtio-run.sh) script provides a wrapper
to correctly launch `qemu-system-x86_64` or `bhyve` on the host system.  Using
it is not required; by default it will print the commands used to setup and
launch the guest VM. You can run these manually if desired.

To launch the unikernel:

    ../scripts/virtio-run/solo5-virtio-run.sh -n tap100 -- test_net.virtio verbose

Use `^C` to terminate the unikernel.

## _virtio_: Running on other hypervisors

The _virtio_ target produces a unikernel that uses the multiboot
protocol for booting. If your hypervisor can boot a multiboot-compliant
kernel directly then this is the preferred method.

If your hypervisor requires a full disk image to boot, you can use the
[solo5-virtio-mkimage](../scripts/virtio-mkimage/solo5-virtio-mkimage.sh) tool to build one.

`solo5-virtio-mkimage` supports the following image formats:

* `raw`: A raw disk image, written out as a sparse file.
* `tar`: A disk image suitable for [uploading to](https://cloud.google.com/compute/docs/tutorials/building-images#publishingimage) Google Compute Engine.

## _virtio_: Limitations

The _virtio_ target was the initial target supported by Solo5 -- while we are
keeping it around as it is useful to some users, it is essentially a
"compatibility layer" providing the Solo5 interface atop "legacy" full-system
virtualization. As the goals of Solo5 have since evolved, we do not expect to
devote a substantial amount of time to the further development of _virtio_.

Therefore, we recommend that new deployments use the _hvt_ target instead.

As the _virtio_ bindings internally support only a single network and/or block
device, if multiple such devices are declared in the application manifest, only
the first valid "acquire" call of each device type will succeed. Conversely, if
multiple such virtual hardware devices are presented to the VM by the
hypervisor, only the first instance of each device will be used by the
unikernel.

The following virtual hardware devices are supported by the _virtio_ target:

* the serial console, fixed at COM1 and 115200 baud
* the KVM paravirtualized clock, if available
* a single virtio network device attached to the PCI bus
* a single virtio block device attached to the PCI bus

Note that _virtio_ does not support ACPI power-off. This can manifest itself in
delays shutting down Solo5 guests running on hypervisors which wait for the
guest to respond to ACPI power-off before performing a hard shutdown.

----

Next: [Debugging Solo5-based unikernels](debugging.md)
