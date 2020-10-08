## 0.6.7 (2020-10-08)

Bug fixes:

* spt: Fix a bug where libseccomp's seccomp\_load() would cause the tender to
  be killed by SIGSYS due to calling free(), which would under certain
  conditions call sbrk(), which is not in our seccomp filter. Work around this
  behaviour by exporting the generated BPF filter and loading it into the
  kernel manually. (#479)
* Various improvements to the Xen bindings for use by MirageOS. (#480, #476).

## 0.6.6 (2020-07-21)

New features:

* This release adds minimal bindings for the Xen hypervisor. Hosts running
  Xen 4.10 or later on x86\_64 are supported, and the bindings build Solo5
  unikernels as PVHv2 domUs only.  These bindings are not a full Solo5 target;
  they exist for the purpose of providing low-level bootstrap code to MirageOS
  and do not provide any network or block I/O functionality.

Bug fixes:

* genode: Do not yield for expired deadlines. (#466)

## 0.6.5 (2020-05-14)

New features:

* Stop host kernels from attempting to execute Solo5 binaries. This improves
  both the user experience on some hosts (e.g. "No such file or directory" vs.
  "Segmentation fault" on Linux) and overall security posture by forcing the
  host kernel ELF loader to bail out earlier rather than actually jumping to
  the unikernel code. (#442)
* hvt: Full support for W^X and correct guest-side page protections on OpenBSD
  6.7+ systems with EPT. (#447)
* hvt: capsicum(4) sandbox for the hvt tender on FreeBSD 12+. (#366)

Bug fixes:

* hvt: Fix hang in `HVT_HYPERCALL_POLL`. On Linux hosts, if `solo5_yield()` was
  called with a deadline that has already passed and the unikernel was not using
  any network devices then the underlying hypercall would hang forever. Not
  known to affect any existing code in production. (#460)

Other notable changes:

* muen: Muen ABI updates, now uses ABI version 2 on the Solo5 side. Muen
  commit f10bd6b or later is required. (#454, #448)
* genode: Support for Genode is limited by toolchain issues and Genode bindings
  are no longer built by default. (#446, see also ocaml/opam-repository#16368)
* Improvements to the build system on BSD/clang hosts. System headers
  (sys/endian.h, osreldate.h) that were mistakenly being installed into the
  Solo5-provided include paths have been removed. For OCaml/MirageOS users,
  ocaml-freestanding 0.6.0 or later is now required. (#453, #455, #457, #461,
  see also mirage/ocaml-freestanding#77)
* Improvements to built-in self tests. (#451)
* Fix build failures with GCC >= 10. (#459)

Known issues:

* Full W^X support / correct guest-side page protections are currently only
  available on the "spt" target on Linux, and the "hvt" target on OpenBSD 6.7
  or later. (#303)
* On OpenBSD, "hvt" operation with multiple network devices results in packet
  loss. This appears to be a bug in kqueue(2) but we have no confirmation from
  upstream. (#374)
* virtio-net is not functional on at least QEMU 5.0 and possibly earlier
  versions. QEMU versions up to and including 3.1.0 are known to work. (#463)

Acknowledgements:

* Thanks to Adam Steen (@adamsteen) for pushing for OpenBSD kernel support for
  manipulating guest EPT mappings, bringing full W^X to hvt on OpenBSD 6.7 or
  later.
* Thanks to Adrian-Ken Rueegsegger (@kensan) for the Muen updates.
* Thanks to Anurag Soni (@anuragsoni) for diagnosing and fixing the build on
  systems with GCC >= 10.
* Thanks to Hannes Mehnert (@hannesm) for diagnosing #460 and for help with
  testing BSD/clang build system changes and generally helping out.
* Thanks to Stefan Grundmann (@sg2342) for the capsicum(4) hvt tender sandbox
  on FreeBSD.

## 0.6.4 (2019-11-14)

This release updates the Genode and Muen bindings, and fixes the following
notable issues:

* Disk images produced by solo5-virtio-mkimage were not bootable due to changes
  in recent SYSLINUX versions (thanks to Ricardo Koller, @ricarkol for finding
  the fix).
* Build failure on FreeBSD 12.1+ (clang 8.0.1) due to issues with TLS in the
  toolchain.

New features:

* "configure.sh" has a new "--only-tools" option, which builds no tenders or
  bindings. In conjunction with "make install-tools", this can be used to
  install "solo5-elftool" only. This is intended for downstream systems which
  need to make use of "solo5-elftool" to query manifest information.
* A "scripts/opam-release.sh" tool has been added to generate OPAM repository
  metadata as part of the Solo5 release process.

Apart from that, this release contains documentation updates and the "hvt"
tender on the aarch64 architecture is now considered production-quality.

## 0.6.3 (2019-10-08)

This release fixes a build problem with Linux distributions such as OpenSUSE
which install libseccomp headers into subdirectories of /usr/include. Note that
this introduces pkg-config as a dependency for building Solo5 on Linux.

No functional changes.

## 0.6.2 (2019-09-23)

This release fixes further OPAM/MirageOS installation problems found by CI, and
the following build problems:

* spt: Support ppc64le on RedHat (thanks to Stefan Berger, @stefanberger).
* Fix check/warning for libseccomp >= 2.3.3 (thanks to Mechiel Lukkien, @mjl-).

No functional changes.

## 0.6.1 (2019-09-19)

This release fixes OPAM/MirageOS installation problems found by CI. No
functional changes.

Additionally, the following entry was missed from the changes for 0.6.0:

* Solo5 tenders, tools and bindings now have an embedded version number, using
  the well-known convention "v0.6.0-4-gc9786d87". Bindings will log the version
  number during start-up. As a consequence of this, Solo5 now needs to be built
  from either a Git tree or a tarball produced by "make distrib". Suitable
  tarballs are uploaded to Github during the release process.

## 0.6.0 (2019-09-19)

This is a major feature release which introduces the concept of an "application
manifest", which enables support for multiple network and block devices.

This release removes the compile-time specialization of the "hvt" tender. While
this was a nice experiment, it is not practical for real-world deployment
scenarios where it is expected that the party supplying the tender (i.e. the
operator / user) will be different to the party supplying the unikernel (i.e.
the developer).

Due to these and other changes, both the public Solo5 APIs (as defined and
documented in solo5.h) and internal tenders/bindings ABI have changed.
Likewise, the build process for Solo5-based unikernels has changed, and
downstream projects will need to be updated.

Please refer to the following list of changes and the Solo5 documentation in
docs/ for details.

New features:

* Application manifest and support for multiple devices. New APIs have been
  added and existing APIs changed. See solo5.h for details. A new tool,
  "solo5-elftool" has been added to generate the binary application manifest
  from JSON source.
* New "spt" (sandboxed process tender) target, allows running Solo5-based
  unikernels as user processes on Linux, using a minimal, strict seccomp-BPF
  sandbox for isolation. Runs on the x86\_64, aarch64 and ppc64le processor
  architectures.
* Compile-time specialization of the "hvt" (hardware virtualized tender) target
  has been removed. Two variants of the "hvt" tender are built, "solo5-hvt" and
  "solo5-hvt-debug" with support for GDB and guest coredumps where available.
* Both the "hvt" and "spt" tenders and bindings now have an internal ABI
  version, and this ABI version is embedded into the unikernel binary. Tenders
  will not run a binary whose ABI version or target does not match.
* Downsteam libOS projects using custom linker scripts for Solo5 will need to
  be updated to handle the ELF NOTEs used to embed the ABI version and
  application manifest in a Solo5 binary.

Security improvements:

* Stack smashing protection is now supported and all bindings, tenders and
  other host tools are built with -fstack-protector-strong. This setting is
  also passed down to MirageOS unikernels via OPAM/pkg-config.
* On Linux, tenders are built with -z noexecstack to enforce a non-executable
  stack.
* Low memory, including the zero page, is now unmapped on "hvt", "spt" and
  "virtio" targets. Tender-internal structures in this memory range are mapped
  read-only.
* Tests for W^X / guest-side page protections have been added and are enforced
  on those targets where this is supported.
* The ELF loader used by tenders has undergone extensive review and
  re-factoring, and fixes for a number of potential security issues have been
  put in place.

Other changes:

* The "hvt" tender now uses epoll(7) on Linux and kqueue(2) on the BSDs for
  network I/O.
* The "hvt" tender "--dumpcore" option now requires a directory, which must
  exist at run time.
* The printf() implementation used by bindings has been replaced with a much
  better implementation from https://github.com/mpaland/printf.
* The `solo5_set_tls_base()` API has been added to give language runtimes that
  wish to manipulate the architecture-specific TLS base register an interface
  for it.
* Many bug fixes, improvements and a lot of code review throughout.
* A C11 compiler is now required, to allow for the use of `_Static_assert()`.

Known issues:

* Full W^X support is currently only possible on the "spt" target. Partial
  support is possible for the "hvt" target running on Linux/KVM hosts. (#303)
* On OpenBSD, "hvt" operation with multiple network devices results in packet
  loss. This appears to be a bug in kqueue(2) but we have no confirmation from
  upstream. (#374)

Acknowledgements:

Thanks to the following new contributors to this release:

* Frithjof Schulze (@schulze)
* Haibo Xu (@xiaobo55x)
* Stefan Berger (@stefanberger)

And finally, a special thank you to C For C's Sake (@cfcs) for extensive code
review.

## 0.5.0 (N/A)

* Skipped for version numbering consistency with downstream MirageOS/Solo5
  components.

## 0.4.1 (2018-11-08)

This release introduces experimental support for the Genode Operating System
Framework as a target for Solo5-based unikernels, and basic support for
dropping "root" privileges in the "hvt" tender on FreeBSD and OpenBSD.

Other changes:

* Migrate OPAM integration to OPAM 2.

Acknowledgements:

Thanks to the following new contributors to this release:

* Emery Hemingway (@ehmry)
* Stefan Grundmann (@sg2342)

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

* Rolf Sommerhalder (@hb9cwp)
* Sven Anderson (@ansiwen)

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

* Adam Steen (@adamsteen)
* Nikhil AP (@niks3089)
* Wei Chen (@Weichen81)

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

* Adrian-Ken Rueegsegger (@Kensan)
* Michael LeMay (@mdlemay)
* Gabriel Jaldon (@gjaldon)

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

* David Kaloper (@pqwy)
* Hannes Mehnert (@hannesm)
* Ian Campbell (@ijc25)
* Sean Grove (@sgrove)

Thank you!

## 0.1.1 (2016-07-21)

* Initial release for publishing to opam.ocaml.org.
