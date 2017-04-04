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
