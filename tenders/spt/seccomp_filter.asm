#define KILL  #0
#define ALLOW #0x7fff0000

/* struct seccomp_data */
#define nr      [0]
#define arch    [4]
#define arg0_lo [16]
#define arg0_hi [20]
#define arg1_lo [24]
#define arg1_hi [28]
#define arg2_lo [32]
#define arg2_hi [36]
#define arg3_lo [40]
#define arg3_hi [44]
#define arg4_lo [48]
#define arg4_hi [52]
#define arg5_lo [56]
#define arg5_hi [60]

/* pread64, pwrite64 and fallocate offsets/sizes */
#define block1_lo arg2_lo
#define block1_hi arg2_hi
#define block2_lo arg3_lo
#define block2_hi arg3_hi

#define BLOCK_JMP(i) jeq $BPF_BLOCK_FD(i), block_fd_##i
#define BLOCK_SIZE(i) \
block_fd_##i: \
        ld $BPF_BLOCK_LO(i) \
        st M[2] \
        ld $BPF_BLOCK_HI(i) \
        st M[3] \
        jmp block_check_range

#define BLOCK_FOREACH(f) \
        f(0) f(1) f(2) f(3) \
        f(4) f(5) f(6) f(7)

start:
        ld arch
        jne $AUDIT_ARCH, kill

        ld nr
        jeq $SYS_pread64, block_call
        jeq $SYS_pwrite64, block_call
        jeq $SYS_clock_gettime, clock_gettime
        jeq $SYS_write, write
        jeq $SYS_read, read
        /*jeq $SYS_fallocate, fallocate*/
        /*jeq $SYS_fdatasync, fdatasync*/
        jeq $SYS_ppoll, ppoll
        jeq $SYS_exit_group, allow
        jmp kill

clock_gettime:
        ld arg0_hi
        jne #0, kill
        ld arg0_lo
        jeq $CLOCK_MONOTONIC, allow
        jeq $CLOCK_REALTIME, allow, kill

ppoll:
        ld arg3_hi
        jne #0, kill  /* no signals */
        ld arg3_lo
        jne #0, kill  /* no signals */
        ld arg4_hi
        jne #0, kill  /* no signals */
        ld arg4_lo
        jne #0, kill  /* no signals */
        ld arg1_hi
        jne #0, kill  /* nfd */
        ld arg1_lo
        jeq $BPF_PPOLL_COUNT, allow, kill /* nfd */

write:
        ld arg0_hi
        jne #0, kill
        ld arg0_lo
        jge #32, kill
        tax
        ld $write_fd_mask
        rsh %x
        jset #1, allow, kill

read:
        ld arg0_hi
        jne #0, kill
        ld arg0_lo
        jge #32, kill
        tax
        ld $read_fd_mask
        rsh %x
        jset #1, allow, kill

/*
fdatasync:
        ld arg0_hi
        jne #0, kill
        ld arg0_lo
        jlt $BPF_BLOCK_MIN, kill
        jge $BPF_BLOCK_MAX, kill, allow

fallocate:
        ld arg0_hi
        jne #0, kill
        ld arg1_lo
        jne $FALLOC_MODE, kill
        jmp block_call
*/

block_call:
        /* check block fd */
        ld arg0_hi
        jne #0, kill
        ld arg0_lo
        jlt $BPF_BLOCK_MIN, kill
        jge $BPF_BLOCK_MAX, kill

        /* check nonnegative, the kernel would also catch that, but we do it earlier */
        ld block1_hi
        and #0x80000000
        jne #0, kill
        ld block2_hi
        and #0x80000000
        jne #0, kill

        /* check alignment */
        ld block1_lo
        and $BPF_BLOCK_MASK
        jne #0, kill
        ld block2_lo
        and $BPF_BLOCK_MASK
        jne #0, kill

        /* M[0] = size_lo + off_lo */
        ld block1_lo
        tax
        ld block2_lo
        add %x
        st M[0]

        /* M[1] = size_hi + off_hi */
        ld block1_hi
        tax
        ld block2_hi
        add %x
        st M[1]

        /* check if low addition overflowed */
        ld block2_lo
        tax
        ld M[0]
        jlt %x, block_ovf
        jmp block_no_ovf

        /* add carry to M[1] */
block_ovf:
        ld M[1]
        add #1
        st M[1]
block_no_ovf:

        /* load sizes to M[2] and M[3] */
        ld arg0_lo
        BLOCK_FOREACH(BLOCK_JMP)
        jmp kill
        BLOCK_FOREACH(BLOCK_SIZE)

block_check_range:
        /* check size_hi + off_hi <= BLOCK_SIZE_HI */
        ld M[1]
        ldx M[3]
        jlt %x, allow
        jgt %x, kill

        /* check size_lo + off_lo <= BLOCK_SIZE_LO */
        ld M[0]
        ldx M[2]
        jgt %x, kill, allow

kill:
        ret KILL

allow:
        ret ALLOW
