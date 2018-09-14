# Debugging Solo5-based unikernels

The _hvt_ target provides support both for live debugging of a running
unikernel, and post-mortem debugging (guest core dumps). In both cases, you
will need to enable building the relevant module in your `solo5-hvt` _tender_.

To do so for the standalone tests provided with Solo5, edit the Makefile and
add the module to the `HVT_MODULES` variable. For other downstream build
systems, consult the documentation of your unikernel project, and/or instruct
your build system to add the module to the invocation of `solo5-hvt-configure`.

## Live debugging

This feature is currently only supported on Linux/KVM on the x86\_64
architecture. The `gdb` module must be included in your build of `solo5-hvt`.

Start solo5-hvt with the `--gdb` flag, like this:

    $ cd tests/test_hello
    $ ./solo5-hvt --gdb test_hello.hvt

This will start a GDB server on TCP port 1234, and the monitor will wait for
GDB to connect before launching the unikernel. You can use the `--gdb-port`
option to specify a different port.

And then from another console start `gdb` and connect to the remote target
listening at `localhost:1234`:

    $ gdb --ex="target remote localhost:1234" test_hello.hvt

Here is a typical gdb session:

    Remote debugging using localhost:1234
    _start (arg=0x5000) at hvt/start.c:24
    24	{
    (gdb) break puts
    Breakpoint 1 at 0x1007e6: puts. (18 locations)
    (gdb) c
    Continuing.

    Breakpoint 1, solo5_app_main (si=si@entry=0x106000 <si>) at test_hello.c:31
    31	    puts("\n**** Solo5 standalone test_hello ****\n\n");
    (gdb) bt
    #0  solo5_app_main (si=si@entry=0x106000 <si>) at test_hello.c:31
    #1  0x00000000001000a7 in _start (arg=0x5000) at hvt/start.c:42
    (gdb) s
    puts (s=0x104708 "\n**** Solo5 standalone test_hello ****\n\n")
        at test_hello.c:26
    26	    solo5_console_write(s, strlen(s));
    (gdb)

## Post-mortem debugging

This feature is currently only supported on Linux/KVM and FreeBSD vmm on the
x86\_64 architecture. The `dumpcore` module must be included in your build of
`solo5-hvt`.

The functionality must also be enabled at run-time by passing the `--dumpcore`
option to `solo5-hvt`. This will cause `solo5-hvt` to generate a core file if
the guest aborts, either due to a trap/fault, or by calling `solo5_abort()`
directly.

You can then load the core file into GDB as follows (this example uses the
`test_abort` provided with Solo5):

    $ gdb test_abort.hvt core.solo5-hvt.11565
    Reading symbols from test_abort.hvt...done.
    [New process 1]
    #0  0x00000000001001af in hvt_do_hypercall (arg=0x1fffffb0, n=10)
        at /home/mato/projects/mirage-solo5/solo5/include/solo5/hvt_abi.h:68
    68	    __asm__ __volatile__("outl %0, %1"
    (gdb) bt
    #0  0x00000000001001af in hvt_do_hypercall (arg=0x1fffffb0, n=10)
        at /home/mato/projects/mirage-solo5/solo5/include/solo5/hvt_abi.h:68
    #1  platform_exit (status=status@entry=255, cookie=cookie@entry=0x0)
        at hvt/platform_lifecycle.c:35
    #2  0x0000000000103293 in solo5_abort () at exit.c:32
    #3  0x0000000000103824 in solo5_app_main (si=si@entry=0x106000 <si>)
        at test_abort.c:32
    #4  0x00000000001000a7 in _start (arg=0x5000) at hvt/start.c:42
    (gdb)

Note that due to the generated core files being somewhat "unusual" from the
point of view of the host system's toolchain, only recent (7.x or newer)
versions of mainline GDB will load them correctly.

----

Next: [Technical overview, goals and limitations, and architecture of Solo5](architecture.md)
