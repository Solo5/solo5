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

* a 64-bit Linux, FreeBSD or OpenBSD system (see also [Supported
  targets](#supported-targets) for further requirements),
* a C11 compiler; recent versions of GCC and clang are supported,
* GNU make,
* full host system headers (on Linux, kernel headers are not always installed
  by default),
* on Linux only, pkg-config and libseccomp >= 2.3.3 are required.

Note that Solo5 does not support cross-compilation. With the exception of the
_muen_ target (which is not self-hosting), you should build
Solo5 and unikernels on a build system matching the host system and processor
architecture you will be running them on.

To build Solo5, run:

```sh
./configure.sh
make # gmake on BSD systems
```

This will build all _targets_ supported by the host system. If you would like
to enable only some _targets_, you can do so by specifying a list to
configure.sh:

```sh
./configure.sh --enable-targets=hvt,spt
make
```

Note that the Solo5 Makefiles do not support building arbitrary `make` targets
(e.g. `bindings` or `tests`).

To run the built-in self-tests (requires `bash` and `coreutils`):

```sh
tests/setup-tests.sh # As root, sets up `tap100` for networking
tests/run-tests.sh   # Root required to run network tests
```

## Supported targets

Supported _targets_, host operating systems/hypervisors and processor
architectures:

Production:

* _hvt_: Linux/KVM, using `solo5-hvt` as a _tender_, on the x86\_64
  architecture.
* _hvt_: FreeBSD vmm, using `solo5-hvt` as a _tender_, on the x86\_64
  architecture.  FreeBSD 11 or later is required, FreeBSD 12 or later is
  recommended for `capsicum(4)` support.
* _hvt_: Linux/KVM, using `solo5-hvt` as a _tender_, on the aarch64
  architecture.  You will need hardware capable of running a recent (v4.14+)
  64-bit mainline kernel and a 64-bit Linux distribution.

Experimental:

* _hvt_: OpenBSD vmm, using `solo5-hvt` as a _tender_, on the x86\_64
  architecture.  OpenBSD 6.4 or later is required, 6.7 or later is recommended
  for full W^X support.
* _spt_: Linux systems on the x86\_64, ppc64le and aarch64 architectures, using
  `solo5-spt` as a _tender_. A Linux distribution with libseccomp >= 2.3.3 is
  required.
* _muen_: The [Muen Separation Kernel](https://muen.sk/), on the x86\_64
  architecture. Muen commit `f10bd6b` or later is required.

Limited:

* _virtio_: Any hypervisor which virtualizes an x86\_64 system with virtio
  devices (e.g.  Linux/KVM with QEMU as a monitor, Google Compute Engine). See
  below under "_virtio_: Limitations" for details.
* _xen_: Xen hypervisor 4.10 or later on x86\_64 only, using the PVHv2 domU
  ABIs. The _xen_ bindings are not a full Solo5 target; they exist for the
  purpose of providing low-level bootstrap code to MirageOS and do not provide
  any network or block I/O functionality.

# Running Solo5-based unikernels

Solo5 itself does not provide a high-level mangement or orchestration layer for
unikernels -- this is intended to be provided by separate downstream projects.
If you are coming from Linux containers you can think of Solo5 as conceptually
occupying the same space in the stack as, for example, `runc`.

If you are looking for a high-level stack for deploying unikernels, one such
project is [Albatross](https://github.com/hannesm/albatross).

## Setting up

The following examples use the standalone
[test\_net](../tests/test_net/test_net.c) unikernel which is
built as part of the normal Solo5 build process.

`test_net` is a minimalist network test which will respond only to ARP
and ICMP echo requests sent to the hard-coded IP address of `10.0.0.2`. It
accepts two possible command line arguments: Either `verbose` for verbose
operation or `limit` to terminate after having sent 100,000 ICMP echo replies.

By convention, we will use the `tap100` interface to talk to the unikernel.

To set up the `tap100` interface on Linux, run (as root):

```sh
ip tuntap add tap100 mode tap
ip addr add 10.0.0.1/24 dev tap100
ip link set dev tap100 up
```

To set up vmm and the `tap100` interface on FreeBSD, run (as root):

```sh
kldload vmm
kldload if_tap
sysctl -w net.link.tap.up_on_open=1
ifconfig tap100 create 10.0.0.1/24 link0
```

To set up vmm and the `tap100` interface on OpenBSD, run (as root):

```sh
cd /dev
./MAKEDEV tap100
ifconfig tap100 inet 10.0.0.1 netmask 255.255.255.0
```

## _hvt_: Running on Linux, FreeBSD and OpenBSD with hardware virtualization

The _hvt_ ("hardware virtualized tender") target supports Linux, FreeBSD and
OpenBSD systems and uses hardware virtualization to isolate the guest
unikernel.

On Linux, the `solo5-hvt` _tender_ only requires access to `/dev/kvm` and
`/dev/net/tun`, and thus does NOT need to run as `root` provided your have
granted the user in question the correct permissions. Most recent Linux
distributions provide a `kvm` group for this purpose, and `/dev/net/tun` is
normally world-writable.

On FreeBSD, `root` privileges are currently required to run the `solo5-hvt`
_tender_ in order to access the `vmm` APIs.

On OpenBSD, the `solo5-hvt` _tender_ must be started as `root`, however it will
drop privileges to the standard `_vmd` user and further use `pledge(2)` to
lower its privileges.

To launch the unikernel, in `tests/test_net/` run:

```sh
../../tenders/hvt/solo5-hvt --mem=2 --net:service0=tap100 -- test_net.hvt verbose
```

Use `^C` to terminate the unikernel.

The option `--mem=2` requests that 2 MB of host memory be allocated, but not
committed, to the unikernel. If it is not specified, a default of of 512 MB is
used.

The option `--net:service0=tap100` requests that the _tender_ attach the network
device with the logical name `service0`, declared in the unikernel's
[application manifest](architecture.md#application-manifest), to the host's TAP
interface named `tap100`.

All devices declared by the unikernel _must_ be attached for it to be allowed
to run. To query a unikernel's _application manifest_ from an existing binary,
you can use `solo5-elftool`:

```sh
../../elftool/solo5-elftool query-manifest test_net.hvt
```

## _spt_: Running on Linux with a strict seccomp sandbox

The _spt_ ("sandboxed process tender") target currently supports Linux systems
only, and uses a _strict_ (minimal whitelist) seccomp sandbox to isolate the
guest unikernel, which runs as a user process on the host.

The `solo5-spt` _tender_ does not require any special privileges to run.

To launch the unikernel, in `tests/test_net/` run:

```sh
../../tenders/spt/solo5-spt --mem=2 --net:service0=tap100 -- test_net.spt verbose
```

Use `^C` to terminate the unikernel.

The `solo5-spt` _tender_ has the same common options as `solo5-hvt`. Refer to
the hvt example in the previous section for a brief description.

## _virtio_: Running with KVM/QEMU on Linux, or bhyve on FreeBSD

The [solo5-virtio-run](../scripts/virtio-run/solo5-virtio-run.sh) script
provides a wrapper to correctly launch `qemu-system-x86_64` or `bhyve` on the
host system.  Using it is not required; by default it will print the commands
used to setup and launch the guest VM. You can run these manually if desired.

To launch the unikernel, in `tests/test_net/` run:

```sh
../../scripts/virtio-run/solo5-virtio-run.sh -n tap100 -- test_net.virtio verbose
```

Use `^C` to terminate the unikernel.

## _virtio_: Running on other hypervisors

The _virtio_ target produces a unikernel that uses the multiboot
protocol for booting. If your hypervisor can boot a multiboot-compliant
kernel directly then this is the preferred method.

If your hypervisor requires a full disk image to boot, you can use the
[solo5-virtio-mkimage](../scripts/virtio-mkimage/solo5-virtio-mkimage.sh) tool
to build one.

`solo5-virtio-mkimage` supports the following image formats:

* `raw`: A raw disk image, written out as a sparse file.
* `tar`: A disk image suitable for [uploading to](https://cloud.google.com/compute/docs/tutorials/building-images#publishingimage) Google Compute Engine.

Note that `solo5-virtio-mkimage` has a fairly specific set of requirements on
the host system, and in general runs only on Linux distributions with
non-broken `syslinux` packages available. For this reason, the script has
built-in support for running as a Docker container using the `-d` option.

For example, to produce a raw disk image containing
the [test\_hello](../tests/test_hello/test_hello.c) unikernel and supply a kernel command line of "Hello, Solo5!", in `tests/test_hello` run:

```sh
../../scripts/virtio-mkimage/solo5-virtio-mkimage.sh -d -f raw test_hello.img test_hello.virtio "Hello, Solo5!"
```

A QEMU command line suitable for booting this image is:

```sh
qemu-system-x86_64 -machine q35 -display none -serial stdio -drive file=test_hello.img,if=virtio,format=raw
```

Use `^C` to terminate the unikernel.

## _virtio_: Limitations

The _virtio_ target was the initial target supported by Solo5 -- while we are
keeping it around as it is useful to some users, it is essentially a
"compatibility layer" providing the Solo5 interface atop "legacy" full-system
virtualization. As the goals of Solo5 have since evolved, we do not expect to
devote a substantial amount of time to the further development of _virtio_.

Therefore, we recommend that new deployments use the _hvt_ or _spt_ target
instead.

As the _virtio_ bindings internally support only a single network and/or block
device, if multiple such devices are declared in the [application
manifest](architecture.md#application-manifest), only the first valid "acquire"
call of each device type will succeed. Conversely, if multiple such virtual
hardware devices are presented to the VM by the hypervisor, only the first
instance of each device type can be used by the unikernel.

The following virtual hardware devices are supported by the _virtio_ target:

* the serial console, fixed at COM1 and 115200 baud
* the KVM paravirtualized clock, if available
* a single virtio network device attached to the PCI bus
* a single virtio block device attached to the PCI bus

Note that _virtio_ does not support ACPI power-off. This can manifest itself in
delays shutting down Solo5 guests running on hypervisors which wait for the
guest to respond to ACPI power-off before performing a hard shutdown.

## _muen_: Running on the Muen Separation Kernel

The _muen_ target provides bindings to run Solo5-based unikernels as native
subjects on the [Muen Separation Kernel](https://muen.sk). Building systems
with Muen is outside of the scope of this guide, for details refer to this
in-depth [article](https://muen.sk/articles.html#mirageos-unikernels).

----

Next: [Debugging Solo5-based unikernels](debugging.md)
