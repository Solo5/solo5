## 0.4.1 (2018-11-08)

This release introduces experimental support for the Genode Operating System
Framework as a target for Solo5-based unikernels, and basic support for
dropping "root" privileges in the "hvt" tender on FreeBSD and OpenBSD.

Other changes:

* Migrate OPAM integration to OPAM 2.

Acknowledgements:

Thanks to the following new contributors to this release:

* Emery Hemingway
* Stefan Grundmann

## 0.4.0 (2018-09-14)

This release is a major restructuring and renaming of Solo5 components,
primarily to reflect that the "ukvm monitor" is no longer specific to the KVM
hypervisor and to allow for future development of further targets and tenders
enabling different sandboxing technologies.

Major changes:

* `kernel/X`: Moved to `bindings/X`, now referred to as the "Solo5 _bindings_
  for X".  Build products are now named `bindings/X/solo5_X.o`.
  * `kernel/solo5.h`: Moved to `include/solo5/solo5.h`.
* _ukvm_: Target has been renamed to _hvt_. Monitor code is now referred to as
  the hvt _tender_ and has been moved to `tenders/hvt/`.
  * `ukvm-configure`: Now named `solo5-hvt-configure`.
  * `ukvm-bin`: Now named `solo5-hvt`.
  * `ukvm/ukvm_guest.h`: Renamed to `include/solo5/hvt_abi.h`.
* Generated VM names used on FreeBSD and OpenBSD have been changed from
  `ukvm%d` to `solo5-%d`, with `%d` being the PID of the `solo5-hvt` tender.
* Core file names produced by the _hvt_ dumpcore module have been changed from
  `core.ukvm.%d` to `core.solo5-hvt.%d`.
* `solo5-run-virtio` and `solo5-mkimage`: Renamed to `solo5-virtio-run` and
  `solo5-virtio-mkimage` respectively.
* OPAM packages used by MirageOS have been renamed from `solo5-kernel-X` to
  `solo5-bindings-X`, accounting for the change from `ukvm` to `hvt`. Full
  details of the impact of this change on existing Mirage/Solo5 installations
  will be provided separately as part of a MirageOS release.

For further details please refer to the discussion and commits merged as part
of #274.

Other changes:

* Update OpenBSD requirements to 6.4 and minor OpenBSD build fixes (#270, #273).

## 0.3.1 (2018-08-10)

Minor point release with build fixes:

* Fix array-bounds compilation error with newer compilers (#266, #267).
* Synchronise with OpenBSD vmmvar.h changes (#268).

Thanks to the following new contributors to this release:

* Rolf Sommerhalder
* Sven Anderson

## 0.3.0 (2018-06-15)

a.k.a. the "longest-overdue release in the history of the project", hopefully
to stay that way.

The biggest user-visible changes by far are the porting of ukvm to run on
FreeBSD and OpenBSD vmm, and the addition of support for the Muen Separation
Kernel as a host system. Oh, and support for ARM64 as a host architecture.

Major changes:

* Internal re-design and refactoring to enable support for multi-{arch,
  host system} (#171, #192).
* ukvm: FreeBSD vmm support (#171).
* ukvm: OpenBSD vmm support (#226).
* Support for Muen Separation Kernel as a host system (#190).
* aarch64 support (#209).
* Much improved support for GDB server in ukvm (#198).
* ukvm: Support for post-mortem debugging (guest core dumps) (#258).
* ukvm: Support for setting guest memory size with --mem (#211).
* Public (unikernel-facing) API refresh, documentation and cleanups (#244,
  #245).
* Self-tests reworked using "bats" framework (#246).

Acknowledgements:

Thanks to the following new contributors to this release:

* Adam Steen
* Nikhil AP
* Wei Chen

## 0.2.2 (2017-04-04)

This release incorporates fixes since the release of MirageOS 3.0.

The changes in #166 may have a security impact, all users are encouraged to
rebuild their deployed unikernels against this release.

Notable changes:

* Add self-test for guest exception handling (#162).
* Setup FPU in ukvm and add self-test for FPU/SSE operation (#163).
* Fix check for KVM 32-bit gap (#165).
* Replace kernel/lib.c with musl-derived implementations (#166).
* solo5-virtio-run.sh support for Arch Linux (#168, #169).

Acknowledgements:

Thanks to the following new contributors to this release:

* Adrian-Ken Rueegsegger
* Michael LeMay
* Gabriel Jaldon

## 0.2.1 (2017-02-21)

Point release to address a compilation issue on systems with Clang 3.9 (#158)
and correct license headers for some source files (#159).

## 0.2.0 (2017-01-26)

This release is targeted for use with MirageOS 3.0.

High-level user-visible changes in this release:

* Support for building the `virtio` backend on FreeBSD.
* The `virtio` drivers are now stable and tested on QEMU/KVM, FreeBSD/bhyve and
  Google Compute Engine.
* A new option (`--net-mac`) for specifying a user-defined MAC address to `ukvm`.
* A new script [solo5-mkimage.sh](tools/mkimage/solo5-mkimage.sh) for building
  MBR-partitioned disk images with a bootloader and Solo5 (`virtio` backed)
  unikernel, with integrated support for building images for Google Compute
  Engine.
* A new script [solo5-run-virtio](tools/run/solo5-run-virtio.sh) for launching
  Solo5 (`virtio` backed) unikernels on QEMU/KVM and FreeBSD/bhyve.
* Improvements to the standalone test programs under `tests/`, including an
  automated test suite.
* Standardised license headers and copyright notices across the codebase and
  introduced an AUTHORS file listing contributors and their affiliations.

Notable changes under the hood:

* Security audit of the ukvm "hypercall" interfaces and ELF loader.
* Cleanup and refactoring of the low-level interrupt and trap handling code.
* Memory layout improvements and unification between the `ukvm` and `virtio`
  backends.
* Many bug fixes.

Acknowledgements:

The following people helped by filing valuable issues, testing and other
non-code contributions to this release:

* David Kaloper
* Hannes Mehnert
* Sean Grove

Thank you!

## 0.1.1 (2016-07-21)

* Initial release for publishing to opam.ocaml.org.
