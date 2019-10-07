This is an end to end "smoketest" for Mirage/Solo5.

**This is a work in progress, may change at any time and likely will be relocated to a different repository.**

TL;DR on how to currently build/run in case I pointed you here:

- `opam install dune shexp lwt` if you don't have them already
- `dune exec bin/main.exe`

This will build the test unikernel in `./unikernel` in a local switch created from scratch, with the package "universe" defined in `./universe` and then attempt to run the smoketest. 

Running the test currently requires sudo access (for ip tuntap) and is Linux-specific for now.

See the comments in the source for more details.
