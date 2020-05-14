                |      ___|
      __|  _ \  |  _ \ __ \
    \__ \ (   | | (   |  ) |
    ____/\___/ _|\___/____/

# About Solo5

Solo5 originally started as a project by Dan Williams at IBM Research to port
MirageOS to run on the Linux/KVM hypervisor. Since then, it has grown into a
more general sandboxed execution environment, suitable for running applications
built using various unikernels (a.k.a. library operating systems), targeting
different sandboxing technologies on diverse host operating systems and
hypervisors.

Some of the unique features of Solo5:

- a public ("guest-facing") [API](include/solo5/solo5.h) designed for ease of
  porting existing and future unikernel-native applications,
- this aforementioned API facilitates the implementation of ("host-facing")
  _bindings_ and _tenders_ designed with isolation, a _minimal attack surface_
  and ease of porting to different sandboxing technologies or
  [host systems](docs/building.md#supported-targets) in mind,
- support for live and post-mortem [debugging](docs/debugging.md) of unikernels,
- fast "boot" times (comparable to loading a standard user process), suitable
  for "function as a service" use-cases.

_Looking for the "ukvm monitor"?_ Since Solo5 0.4.0, our terminology has
changed to better reflect the intended [architecture](docs/architecture.md) and
long-term goals of the project.  What used to be referred to as a _monitor_ is
now referred to as a _tender_. As part of this change, the _ukvm_ target and
_monitor_ have been renamed to _hvt_ ("hardware virtualized _tender_") to
reflect that they are no longer specific to the KVM hypervisor, and to allow
for development of further _tenders_ such as _spt_.

# Getting started

As Solo5 is essentially a piece of "middleware" interfacing unikernel-style
applications with their host systems, it is not an end-developer product as
such.

To get started as a developer with Solo5, please refer primarily to the
instructions provided by the unikernel project you intend to develop
applications with:

- MirageOS: https://mirage.io/wiki/install
- IncludeOS: http://www.includeos.org/

That said, we provide the following documentation, not specific to any
unikernel in particular:

- [Building Solo5 and running Solo5-based unikernels](docs/building.md)
- [Debugging Solo5-based unikernels](docs/debugging.md)
- [Technical overview, goals and limitations, and architecture of Solo5](docs/architecture.md)

# Contributing and community

Solo5 is developed on GitHub and licensed under a liberal ISC license. We
accept contributions via GitHub pull requests. When submitting a contribution,
please add your details to the `AUTHORS` file, and if your contribution adds
new source files copy the copyright header from an existing source file.

The coding style for the project is "as for the Linux kernel, but with 4
spaces instead of tabs". When in doubt, please follow style in existing source
files.

We operate a mailing list for general Solo5 development discussion, at
solo5@lists.h3q.com. To subscribe to the list, send an empty email to
solo5-subscribe@lists.h3q.com. Archives are available at [The Mail
Archive](https://www.mail-archive.com/solo5@lists.h3q.com/).

If you are considering a substantial contribution to Solo5, would like to port
a new unikernel to Solo5, or have general questions unrelated to a specific
unikernel, please get in touch via the mailing list.
