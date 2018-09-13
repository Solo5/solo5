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

Additionally, Solo5 introduces the concept of a _tender_, which is loosely the
equivalent of QEMU in the standard KVM/QEMU setup. The [hvt](../tenders/hvt/)
_tender_ is designed to be modular from the outset, and is several orders of
magnitude smaller in code size than QEMU. (See the original ukvm
[paper](https://www.usenix.org/system/files/conference/hotcloud16/hotcloud16_williams.pdf)
from Hotcloud 2016 for a detailed comparison).

Note that both the hvt _tender_ and _target_ were formerly known as the "ukvm
monitor", or just ukvm. As of the Solo5 0.4.0 release, these have been renamed
to reflect that they are no longer specific to the Linux/KVM hypervisor, and to
allow for development of further _tenders_ using different sandboxing
technologies.

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

The current architecture is limited to supporting a single network interface
and block device per unikernel instance. For details on why this is so, and
what needs to happen for this limitation to be lifted, please see this mailing
list [thread](https://www.mail-archive.com/solo5@lists.h3q.com/msg00024.html).

The Solo5 interfaces and ABI are not yet stable, and will change in the future.
We expect to provide a stable ABI (binary compatibility) in the future.

# Architecture and code organisation

The main components of Solo5 are:

- [bindings/](../bindings/): the Solo5 _bindings_ to (implementation of) the
  unikernel-facing interface for the various supported targets, as defined in
  [solo5.h](../include/solo5/solo5.h).
- [tenders/hvt/](../tenders/hvt/): the tender implementation for the _hvt_
  target, with tender-internal interfaces defined in
  [hvt.h](../tenders/hvt/hvt.h) and the internal isolation ("hypercall")
  interface to Solo5 defined in [hvt\_abi.h](../include/solo5/hvt_abi.h). All
  non-core code is contained in optional modules, which are selected at
  unikernel build time by the
  [solo5-hvt-configure](../tenders/hvt/solo5-hvt-configure) script.
- [tests/](../tests/): self tests used as part of our CI system.
- [scripts/](../scripts/): extra tooling and scripts (mainly to support the
  _virtio_ target).

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
