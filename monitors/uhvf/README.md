This is the beginning of a port of ukvm to Hypervisor.framework on
MacOSX, so that Solo5/Mirage-based unikernels can run natively on that
system.  The goal is for ukvm to end up with both a KVM-based bottom
half and Hypervisor.framework-based bottom half, whereas the top half
is shared.

Solo5 doesn't build properly in OSX yet (although @hannesm has done a
bunch of work to make it build on FreeBSD, so I don't think it's far
off), so I build using Docker for Mac with a simple build container. I
also use containers to build Mirage unikernels.  See
https://github.com/djwillia/dockerfiles.

At the moment, uhvf can do the Solo5 hello test and ping_serve test
and also run the Mirage console, stackv4, and block test (from
mirage-skeleton).  At this point, uhvf should have all the features of
ukvm.  

For networking, I'm using the `vmnet` framework.  We can test ping by
running the test_ping_serve unikernel:

    sudo ./uhvf ../tests/test_ping_serve/test_ping_serve.ukvm

And configure the host to know how to ping it like this (also in
`net-setup.bash`):

    BRIDGE=`ifconfig -l |grep -o bridge[0-9]* |tail -n 1`
    IF=`ifconfig -l |grep -o en[0-9]* |tail -n 1`
    sudo ifconfig $BRIDGE 10.0.0.1/24 -hostfilter $IF

Then:

    ping 10.0.0.2

GDB also works, but it's a little bit weird because a gdb compiled and
configured for Mac doesn't necessarily understand the ELF format that
the .ukvm unikernel is in.  For this reason, I use a gdb in a Docker
container as the gdb console.  For example, this unikernel:

    sudo ./uhvf --disk=../tests/test_blk/disk.img --gdb ../tests/test_blk/test_blk.ukvm 

is debugged with (again see https://github.com/djwillia/dockerfiles):

    gdb-docker test_blk.ukvm 

with the gdb command:

    target remote $IP:1234

where IP is the *external* network interface's IP address.  (localhost
doesn't work because of how the container networking currently is
done).

Older notes:

- KVM doesn't allow a trap on `rdtsc` but it should if we want to use
  the same interface for ukvm and uhvf (for e.g., det replay).  

- It looks like the PVCLOCK can be completely removed from the ukvm
  parts of Solo5, as long as we change the poll hypercall to send the
  `until_nsecs` directly

- All interrupt handlers can be removed from the solo5 parts of ukvm
  because we get to see what exception happened in uhvf





