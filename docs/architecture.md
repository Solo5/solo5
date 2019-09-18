# Technical overview

Solo5 is a sandboxed execution environment primarily intended for, but not
limited to, running applications built using various unikernels (a.k.a.
library operating systems).

Conceptually, if you think of a unikernel as a user process, albeit one that
does more in "userspace", Solo5 re-defines the
[interface](../include/solo5/solo5.h) between the process and its host
operating system or hypervisor in a way that is designed to:

- be as "legacy-free" and "thin"(\*) as possible, thus having a _minimal attack
  surface_, making it much easier to reason about the isolation properties of
  Solo5-based applications,

  (\*) compared to existing interfaces, such as that exposed to a Linux process
  by the kernel, or to a full-system virtual machine by QEMU

- make it easy to implement new _targets_, targeting a variety of different
  sandboxing technologies (e.g. hardware virtualization, Linux seccomp, Intel
  SGX), host operating systems and hypervisors. Solo5-based applications can
  then be straightforwardly deployed on any supported target without the need
  to modify their source code.

It turns out that simplifying this interface has the following desirable
properties:

- removal of interrupts implies more deterministic behaviour of applications,
  allowing for efficient record/replay and debugging,
- fast "boot" times, comparable to loading a standard user process, suitable
  for "function as a service" use-cases,
- ease of porting existing and future unikernels to run on top of the Solo5
  interface.

Additionally, Solo5 introduces the concept of a _tender_, which, for _targets_
that make use of one, is the component responsible for "tending to" the
guest at load and/or run time:

- In the case of _hvt_ ("hardware virtualized tender"): the _tender_ is loosely
  equivalent to QEMU in a typical KVM/QEMU setup. The [solo5-hvt](../tenders/hvt/)
  _tender_ is designed to be modular from the outset, and is several orders of
  magnitude smaller in code size than QEMU. See the original ukvm
  [paper](https://www.usenix.org/system/files/conference/hotcloud16/hotcloud16_williams.pdf)
  from Hotcloud 2016 for a detailed comparison.

- In the case of _spt_ ("sandboxed process tender"): the
  [solo5-spt](../tenders/spt) _tender_ is responsible for loading the guest
  into memory, installing a seccomp sandbox and passing control to the guest.
  The seccomp filter installed by _spt_ is a _strict whitelist_ allowing access
  to the minimal set of system calls required to run the guest (as of this
  writing, 7 distinct system calls in total if all devices are configured). See
  our SoCC'18 [paper](https://dl.acm.org/citation.cfm?id=3267845) for a
  discussion of the security properties of the initial implementation of this
  concept.

_Note_: Both the hvt _tender_ and _target_ were formerly known as the "ukvm
monitor", or just ukvm. As of the Solo5 0.4.0 release, these have been renamed
to reflect that they are no longer specific to the Linux/KVM hypervisor, and to
allow for development of further _tenders_ such as _spt_.

## Goals

The Solo5 implementation strives to be _minimalist_ and _correct_, with an
emphasis on working out good external, internal and user/operator interfaces in
preference to (prematurely) adding features or scope creep.

## Non-goals

- SMP is not supported. This is intentional, Solo5-based applications should
  scale out by running multiple instances of the application,
- no interrupts or preemptive scheduling, in fact no scheduling at all is
  provided at the Solo5 level. This reduces complexity and removes the problem
  of "scheduling a scheduler" while fitting in with the prevalent model of
  current unikernels, which use co-operative scheduling internally,
- Solo5 does not run on bare metal (without a host operating system or
  hypervisor).

## Current status and limitations

Solo5 has been in development since late 2015, and is suitable for early
adopters. A variety of different _[targets](building.md#supported-targets)_ are
supported. Production deployments of Solo5-based unikernels exist on Linux/KVM,
FreeBSD/vmm and Google Compute Engine.

The current interface and its implementations have not been designed for high
I/O performance. Proposed prototypes show that I/O performance comparable or
better than that of virtual machines using virtio-style devices is possible.

The Solo5 interfaces and ABI are not yet stable, and will change in the future.
We expect to provide a stable ABI (binary compatibility) in the future.

# Architecture

## Application manifest

Solo5 introduces the concept of an _application manifest_, which is defined by
the developer at unikernel build time, using the following JSON format and
customarily named `manifest.json`:

```jsonc
{
  "type": "solo5.manifest",
  "version": 1,
  "devices": [
    { "name": "NAME", "type": "TYPE" }
    // ... up to 63 user-specified devices ...
  ]
}
```

_NAME_ is a unique logical identifier (a.k.a. "pet name") for the device. The
intention is that the name conveys some meaning of the intended use ("wiring")
of the device, eg. `frontend` for a network or `storage` for a block device.

_NAME_ must be composed of alphanumeric characters only, and within 1..67
characters in length.

_TYPE_ is the type of device being declared, currently `BLOCK_BASIC` or
`NET_BASIC`.

Note that there is a maximum limit of 63 user-specified devices in the manifest.

At unikernel build time, `manifest.json` is pre-processed by `solo5-elftool`,
generating a C source file with a binary representation. This source file is
then compiled using `cc` from the Solo5 toolchain, and linked into the
unikernel binary, where it is represented as an ELF "NOTE".

Additionally, `solo5-elftool` provides the `query-manifest` subcommand to
extract the binary manifest from a unikernel binary and display it as JSON.

## Public API, _Tenders_ and _Bindings_

The _bindings_ implement the Solo5 public ("unikernel-facing") API defined in
[include/solo5/solo5.h](include/solo5/solo5.h) for a particular _target_.

The _hvt_ and _spt_ targets make use of a _tender_ which serves as a loader for
the unikernel within the host OS, sets up access to host resources declared in
the _application manifest_ and, in the case of _hvt_, mediates guest access to
those resources.

## Code organisation

The main components of Solo5 are:

- [bindings/](../bindings/): the Solo5 _bindings_ to (implementation of) the
  unikernel-facing interface for the various supported targets, as defined in
  [solo5.h](../include/solo5/solo5.h).
- [tenders/common/](../tenders/common): common code shared by _tender_
  implementations, notably including the ELF loader and _application manifest_
  validation routines. The latter is also indirectly included by the bindings
  implementation.
- [tenders/hvt/](../tenders/hvt/): the _tender_ implementation for the _hvt_
  target, with tender-internal interfaces defined in
  [hvt.h](../tenders/hvt/hvt.h) and the internal "hypercall" ABI
  to Solo5 defined in [hvt\_abi.h](../include/solo5/hvt_abi.h).
- [tenders/spt](../tenders/spt/): the _tender_ implementation for the _spt_
  target. Tender-internal interfaces are defined in [spt.h](../tenders/spt/spt.h)
  and internal Solo5-facing ABIs in [spt\_abi.h](../include/solo5/spt_abi.h).
- [elftool/](../elftool): a tool for _application manifest_ generation and
  extracting information from Solo5 binaries.
- [tests/](../tests/): self tests used as part of our CI system.
- [scripts/](../scripts/): extra tooling and scripts (mainly to support the
  _virtio_ target).
- [opam/](../opam/): package definitions and pkg-config files for integration
  with OPAM and MirageOS.

The code is architected to split processor architecture and/or host operating
system (_target_)-specific modules from shared functionality where possible.
However, it is acceptable to make minimal, judicious use of `#ifdef` where
doing otherwise would result in excessive code duplication.

Due to the need to run automated tests on bare metal, our CI system is a custom
solution, the details of which can be found in a dedicated
[repository](https://github.com/Solo5/solo5-ci). The scripts run by the CI
system on every GitHub pull request can be found in [build.sh](../build.sh). We
also use Travis CI as a "backup", with the corresponding configuration in
[.travis.yml](../.travis.yml).
