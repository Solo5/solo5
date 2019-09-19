# Debugging Solo5-based unikernels

The _hvt_ target provides support both for live debugging of a running
unikernel, and post-mortem debugging (guest core dumps). In both cases, you
will need to run your unikernel using the `solo5-hvt-debug` _tender_, which
enables the `gdb` and/or `dumpcore` modules with debugging functionality.

The _spt_ target likewise provides this support, without the need for a special
`-debug` _tender_. See below for details.

## Live debugging of _hvt_ unikernels

This feature is currently only supported on Linux/KVM on the x86\_64
architecture.

Start the `solo5-hvt-debug` _tender_ with the `--gdb` flag, like this:

```sh
cd tests/test_hello
../../tenders/hvt/solo5-hvt-debug --gdb test_hello.hvt
```

This will start a GDB server on TCP port 1234, and the _tender_ will wait for
GDB to connect before launching the unikernel. You can use the `--gdb-port`
option to specify a different port.

And then from another console start `gdb` and connect to the remote target
listening at `localhost:1234`:

```sh
cd tests/test_hello
gdb --ex="target remote localhost:1234" test_hello.hvt
```

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

## Post-mortem debugging of _hvt_ unikernels

This feature is currently only supported on Linux/KVM and FreeBSD vmm on the
x86\_64 architecture.

The functionality must be enabled at run-time by passing the
`--dumpcore=DIR` option to `solo5-hvt-debug`. This will cause `solo5-hvt-debug`
to generate a core file in DIR if the guest aborts, either due to a trap/fault,
or by calling `solo5_abort()` directly.

Using the [test\_dumpcore](../tests/test_dumpcore/test_dumpcore.c) provided
with Solo5 as an example:

```
$ ../../tenders/hvt/solo5-hvt-debug --dumpcore=$PWD test_dumpcore.hvt
            |      ___|
  __|  _ \  |  _ \ __ \
\__ \ (   | | (   |  ) |
____/\___/ _|\___/____/
Solo5: Bindings version v0.4.1-208-g0f901b7
Solo5: Memory map: 512 MB addressable:
Solo5:   reserved @ (0x0 - 0xfffff)
Solo5:       text @ (0x100000 - 0x103fff)
Solo5:     rodata @ (0x104000 - 0x105fff)
Solo5:       data @ (0x106000 - 0x10afff)
Solo5:       heap >= 0x10b000 < stack < 0x20000000

**** Solo5 standalone test_dumpcore ****

Solo5: solo5_abort() called
solo5-hvt-debug: dumpcore: dumping guest core to: /home/mato/projects/mirage-solo5/solo5/tests/test_dumpcore/core.solo5-hvt.21397
solo5-hvt-debug: dumpcore: dumped 18 pages of total 131072 pages
$ gdb -q test_dumpcore.hvt core.solo5-hvt.21397
Reading symbols from test_dumpcore.hvt...done.
[New process 1]
#0  0x000000000010327f in hvt_do_hypercall (arg=0x1fffffa8, n=8)
    at /home/mato/projects/mirage-solo5/solo5/include/solo5/hvt_abi.h:75
75	    __asm__ __volatile__("outl %0, %1"
(gdb) bt
#0  0x000000000010327f in hvt_do_hypercall (arg=0x1fffffa8, n=8)
    at /home/mato/projects/mirage-solo5/solo5/include/solo5/hvt_abi.h:75
#1  platform_exit (status=status@entry=255, cookie=cookie@entry=0x0)
    at hvt/platform_lifecycle.c:35
#2  0x0000000000102cd3 in solo5_abort () at exit.c:32
#3  0x0000000000103dc4 in solo5_app_main (si=si@entry=0x107000 <si>)
    at test_dumpcore.c:32
#4  0x00000000001000f1 in _start (arg=0x10000) at hvt/start.c:49
(gdb)
```

Note that due to the generated core files being somewhat "unusual" from the
point of view of the host system's toolchain, only recent (7.x or newer)
versions of mainline GDB will load them correctly.

## Live debugging of _spt_ unikernels

Unikernels built for the _spt_ target can be debugged using a standard Linux
GDB, keeping the following points in mind:

1. You should launch GDB against the `solo5-spt` _tender_, not the unikernel
   binary.
2. The guest unikernel will be loaded at `0x100000`. Use `add-symbol-file` to
   instruct GDB to load symbols from the unikernel binary, with a load address
   of `0x100000`.
3. In order to set breakpoints in the guest unikernel, the binary must already
   have been loaded into memory by the _tender_. The best way to accomplish this
   is to first set a breakpoint on `spt_launch` in `solo5-spt`, continue
   execution to it and only then set breakpoints in the unikernel.

An example GDB session using this approach:

```
$ gdb -q --args tenders/spt/solo5-spt tests/test_hello/test_hello.spt
Reading symbols from tenders/spt/solo5-spt...done.
(gdb) b spt_launch
Breakpoint 1 at 0x20fa
(gdb) r
Starting program: /home/mato/projects/mirage-solo5/solo5/tenders/spt/solo5-spt tests/test_hello/test_hello.spt

Breakpoint 1, 0x00005555555560fa in spt_launch ()
(gdb) add-symbol-file tests/test_hello/test_hello.spt 0x100000
add symbol table from file "tests/test_hello/test_hello.spt" at
        .text_addr = 0x100000
(y or n) y
Reading symbols from tests/test_hello/test_hello.spt...done.
(gdb) b solo5_app_main
Breakpoint 2 at 0x1022d0: file test_hello.c, line 30.
(gdb) c
Continuing.
            |      ___|
  __|  _ \  |  _ \ __ \
\__ \ (   | | (   |  ) |
____/\___/ _|\___/____/
Solo5: Memory map: 512 MB addressable:
Solo5:   reserved @ (0x0 - 0xfffff)
Solo5:       text @ (0x100000 - 0x102fff)
Solo5:     rodata @ (0x103000 - 0x103fff)
Solo5:       data @ (0x104000 - 0x105fff)
Solo5:       heap >= 0x106000 < stack < 0x20000000

Breakpoint 2, solo5_app_main (si=si@entry=0x105040 <si>) at test_hello.c:30
30	{
(gdb)
```

----

Next: [Technical overview, goals and limitations, and architecture of Solo5](architecture.md)
