# Debugging Solo5-based unikernels

The _ukvm_ target provides support both for live debugging of a running
unikernel, and post-mortem debugging (guest core dumps). In both cases, you
will need to enable building the relevant module in your `ukvm-bin` _monitor_.

To do so for the standalone tests provided with Solo5, edit the Makefile and
add the module to the `UKVM_MODULES` variable. For other downstream build
systems, consult the documentation of your unikernel project, and/or instruct
your build system to add the module to the invocation of `ukvm-configure`.

## Live debugging

This feature is currently only supported on Linux/KVM on the x86\_64
architecture. The `gdb` module must be included in your build of `ukvm-bin`.

Start ukvm with the `--gdb` flag, like this:

    $ cd tests/test_hello
    $ ./ukvm-bin --gdb test_hello.ukvm

This will start a GDB server on TCP port 1234, and the monitor will wait for
GDB to connect before launching the unikernel. You can use the `--gdb-port`
option to specify a different port.

And then from another console start `gdb` and connect to the remote target
listening at `localhost:1234`:

    $ gdb --ex="target remote localhost:1234" test_hello.ukvm

Here is a typical gdb session:

    (gdb) break puts
    Breakpoint 1 at 0x100530: puts. (2 locations)
    (gdb) c
    Continuing.

    Breakpoint 1, puts (s=s@entry=0x107808 "\n**** Solo5 standalone test_hello ****\n\n") at test_hello.c:25
    25	{
    (gdb) bt
    #0  puts (s=s@entry=0x107808 "\n**** Solo5 standalone test_hello ****\n\n") at test_hello.c:25
    #1  0x0000000000106a8e in solo5_app_main (cmdline=cmdline@entry=0x6000 "") at test_hello.c:31
    #2  0x0000000000100089 in _start (arg=0x5000) at ukvm/kernel.c:42
    (gdb) s
    26	    solo5_console_write(s, strlen(s));
    (gdb)

## Post-mortem debugging

This feature is currently only supported on Linux/KVM and FreeBSD vmm on the
x86\_64 architecture. The `dumpcore` module must be included in your build of
`ukvm-bin`.

The functionality must also be enabled at run-time by passing the `--dumpcore`
option to `ukvm-bin`. This will cause `ukvm-bin` to generate a core file if the
guest aborts, either due to a trap/fault, or by calling `solo5_abort()`
directly.

You can then load the core file into GDB as follows (this example uses the
`test_abort` provided with Solo5):

    $ gdb test_abort.ukvm core.ukvm.11509
    Reading symbols from ./test_abort.ukvm...done.
    [New process 1]
    #0  0x00000000001001af in ukvm_do_hypercall (arg=0x1fffffb0, n=10) at ukvm/ukvm_guest.h:68
    68          __asm__ __volatile__("outl %0, %1"
    (gdb) bt
    #0  0x00000000001001af in ukvm_do_hypercall (arg=0x1fffffb0, n=10) at ukvm/ukvm_guest.h:68
    #1  platform_exit (status=status@entry=255, cookie=cookie@entry=0x0) at ukvm/platform_lifecycle.c:35
    #2  0x0000000000103293 in solo5_abort () at exit.c:32
    #3  0x0000000000103824 in solo5_app_main (si=si@entry=0x106000 <si>) at test_abort.c:32
    #4  0x00000000001000a7 in _start (arg=0x5000) at ukvm/kernel.c:42
    (gdb)

Note that due to the generated core files being somewhat "unusual" from the
point of view of the host system's toolchain, only recent (7.x or newer)
versions of mainline GDB will load them correctly.

----

Next: [Technical overview, goals and limitations, and architecture of Solo5](architecture.md)
