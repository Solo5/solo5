{ 0x20,  0,  0, 0x00000004 },
{ 0x15,  0,  0, AUDIT_ARCH },
{ 0x20,  0,  0, 0000000000 },
{ 0x15, 40,  0, SYS_pread64 },
{ 0x15, 39,  0, SYS_pwrite64 },
{ 0x15,  5,  0, SYS_clock_gettime },
{ 0x15, 21,  0, SYS_write },
{ 0x15, 28,  0, SYS_read },
{ 0x15,  7,  0, SYS_ppoll },
{ 0x15, 127,  0, SYS_exit_group },
{ 0x05,  0,  0, 0x0000007d },
{ 0x20,  0,  0, 0x00000014 },
{ 0x15,  0, 123, 0000000000 },
{ 0x20,  0,  0, 0x00000010 },
{ 0x15, 122,  0, CLOCK_MONOTONIC },
{ 0x15, 121, 120, CLOCK_REALTIME },
{ 0x20,  0,  0, 0x0000002c },
{ 0x15,  0, 118, 0000000000 },
{ 0x20,  0,  0, 0x00000028 },
{ 0x15,  0, 116, 0000000000 },
{ 0x20,  0,  0, 0x00000034 },
{ 0x15,  0, 114, 0000000000 },
{ 0x20,  0,  0, 0x00000030 },
{ 0x15,  0, 112, 0000000000 },
{ 0x20,  0,  0, 0x0000001c },
{ 0x15,  0, 110, 0000000000 },
{ 0x20,  0,  0, 0x00000018 },
{ 0x15, 109, 108, BPF_PPOLL_COUNT },
{ 0x20,  0,  0, 0x00000014 },
{ 0x15,  0, 106, 0000000000 },
{ 0x20,  0,  0, 0x00000010 },
{ 0x35, 104,  0, 0x00000020 },
{ 0x07,  0,  0, 0000000000 },
{ 0000,  0,  0, write_fd_mask },
{ 0x7c,  0,  0, 0000000000 },
{ 0x45, 101, 100, 0x00000001 },
{ 0x20,  0,  0, 0x00000014 },
{ 0x15,  0, 98, 0000000000 },
{ 0x20,  0,  0, 0x00000010 },
{ 0x35, 96,  0, 0x00000020 },
{ 0x07,  0,  0, 0000000000 },
{ 0000,  0,  0, read_fd_mask },
{ 0x7c,  0,  0, 0000000000 },
{ 0x45, 93, 92, 0x00000001 },
{ 0x20,  0,  0, 0x00000014 },
{ 0x15,  0, 90, 0000000000 },
{ 0x20,  0,  0, 0x00000010 },
{ 0x35,  0, 88, BPF_BLOCK_MIN },
{ 0x35, 87,  0, BPF_BLOCK_MAX },
{ 0x20,  0,  0, 0x00000024 },
{ 0x54,  0,  0, 0x80000000 },
{ 0x15,  0, 84, 0000000000 },
{ 0x20,  0,  0, 0x0000002c },
{ 0x54,  0,  0, 0x80000000 },
{ 0x15,  0, 81, 0000000000 },
{ 0x20,  0,  0, 0x00000020 },
{ 0x54,  0,  0, BPF_BLOCK_MASK },
{ 0x15,  0, 78, 0000000000 },
{ 0x20,  0,  0, 0x00000028 },
{ 0x54,  0,  0, BPF_BLOCK_MASK },
{ 0x15,  0, 75, 0000000000 },
{ 0x20,  0,  0, 0x00000020 },
{ 0x07,  0,  0, 0000000000 },
{ 0x20,  0,  0, 0x00000028 },
{ 0x0c,  0,  0, 0000000000 },
{ 0x02,  0,  0, 0000000000 },
{ 0x20,  0,  0, 0x00000024 },
{ 0x07,  0,  0, 0000000000 },
{ 0x20,  0,  0, 0x0000002c },
{ 0x0c,  0,  0, 0000000000 },
{ 0x02,  0,  0, 0x00000001 },
{ 0x20,  0,  0, 0x00000028 },
{ 0x07,  0,  0, 0000000000 },
{ 0x60,  0,  0, 0000000000 },
{ 0x3d,  0,  1, 0000000000 },
{ 0x05,  0,  0, 0x00000003 },
{ 0x60,  0,  0, 0x00000001 },
{ 0x04,  0,  0, 0x00000001 },
{ 0x02,  0,  0, 0x00000001 },
{ 0x20,  0,  0, 0x00000010 },
{ 0x15,  8,  0, BPF_BLOCK_FD(0) },
{ 0x15, 12,  0, BPF_BLOCK_FD(1) },
{ 0x15, 16,  0, BPF_BLOCK_FD(2) },
{ 0x15, 20,  0, BPF_BLOCK_FD(3) },
{ 0x15, 24,  0, BPF_BLOCK_FD(4) },
{ 0x15, 28,  0, BPF_BLOCK_FD(5) },
{ 0x15, 32,  0, BPF_BLOCK_FD(6) },
{ 0x15, 36,  0, BPF_BLOCK_FD(7) },
{ 0x05,  0,  0, 0x0000002f },
{ 0000,  0,  0, BPF_BLOCK_LO(0) },
{ 0x02,  0,  0, 0x00000002 },
{ 0000,  0,  0, BPF_BLOCK_HI(0) },
{ 0x02,  0,  0, 0x00000003 },
{ 0x05,  0,  0, 0x00000023 },
{ 0000,  0,  0, BPF_BLOCK_LO(1) },
{ 0x02,  0,  0, 0x00000002 },
{ 0000,  0,  0, BPF_BLOCK_HI(1) },
{ 0x02,  0,  0, 0x00000003 },
{ 0x05,  0,  0, 0x0000001e },
{ 0000,  0,  0, BPF_BLOCK_LO(2) },
{ 0x02,  0,  0, 0x00000002 },
{ 0000,  0,  0, BPF_BLOCK_HI(2) },
{ 0x02,  0,  0, 0x00000003 },
{ 0x05,  0,  0, 0x00000019 },
{ 0000,  0,  0, BPF_BLOCK_LO(3) },
{ 0x02,  0,  0, 0x00000002 },
{ 0000,  0,  0, BPF_BLOCK_HI(3) },
{ 0x02,  0,  0, 0x00000003 },
{ 0x05,  0,  0, 0x00000014 },
{ 0000,  0,  0, BPF_BLOCK_LO(4) },
{ 0x02,  0,  0, 0x00000002 },
{ 0000,  0,  0, BPF_BLOCK_HI(4) },
{ 0x02,  0,  0, 0x00000003 },
{ 0x05,  0,  0, 0x0000000f },
{ 0000,  0,  0, BPF_BLOCK_LO(5) },
{ 0x02,  0,  0, 0x00000002 },
{ 0000,  0,  0, BPF_BLOCK_HI(5) },
{ 0x02,  0,  0, 0x00000003 },
{ 0x05,  0,  0, 0x0000000a },
{ 0000,  0,  0, BPF_BLOCK_LO(6) },
{ 0x02,  0,  0, 0x00000002 },
{ 0000,  0,  0, BPF_BLOCK_HI(6) },
{ 0x02,  0,  0, 0x00000003 },
{ 0x05,  0,  0, 0x00000005 },
{ 0000,  0,  0, BPF_BLOCK_LO(7) },
{ 0x02,  0,  0, 0x00000002 },
{ 0000,  0,  0, BPF_BLOCK_HI(7) },
{ 0x02,  0,  0, 0x00000003 },
{ 0x05,  0,  0, 0000000000 },
{ 0x60,  0,  0, 0x00000001 },
{ 0x61,  0,  0, 0x00000003 },
{ 0x3d,  0,  5, 0000000000 },
{ 0x2d,  3,  0, 0000000000 },
{ 0x60,  0,  0, 0000000000 },
{ 0x61,  0,  0, 0x00000002 },
{ 0x2d,  0,  1, 0000000000 },
{ 0x06,  0,  0, 0000000000 },
{ 0x06,  0,  0, 0x7fff0000 },
