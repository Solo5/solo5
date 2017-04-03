/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of ukvm, a unikernel monitor.
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#if defined(__x86_64__) && defined(__linux__)

/***************************************************************************
 *
 *                THIS SOFTWARE IS NOT COPYRIGHTED
 *
 * HP offers the following for use in the public domain.  HP makes no
 * warranty with regard to the software or it's performance and the
 * user accepts the software "AS IS" with all faults.
 *
 * HP DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD
 * TO THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 **************************************************************************/

/****************************************************************************
 *  Header: remcom.c,v 1.34 91/03/09 12:29:49 glenne Exp $
 *
 *  Module name: remcom.c $
 *  Revision: 1.34 $
 *  Date: 91/03/09 12:29:49 $
 *  Contributor:     Lake Stevens Instrument Division$
 *
 *  Description:     low level support for gdb debugger. $
 *
 *  Considerations:  only works on target hardware $
 *
 *  Written by:      Glenn Engel $
 *  ModuleState:     Experimental $
 *
 *  NOTES:           See Below $
 *
 *  Modified for 386 by Jim Kingdon, Cygnus Support.
 *
 *  To enable debugger support, two things need to happen.  One, a
 *  call to set_debug_traps() is necessary in order to allow any breakpoints
 *  or error conditions to be properly intercepted and reported to gdb.
 *  Two, a breakpoint needs to be generated to begin communication.  This
 *  is most easily accomplished by a call to breakpoint().  Breakpoint()
 *  simulates a breakpoint by executing a trap #1.
 *
 *  The external function exceptionHandler() is
 *  used to attach a specific handler to a specific 386 vector number.
 *  It should use the same privilege level it runs at.  It should
 *  install it as an interrupt gate so that interrupts are masked
 *  while the handler runs.
 *
 *  Because gdb will sometimes write to the stack area to execute function
 *  calls, this program cannot rely on using the supervisor stack so it
 *  uses it's own stack area reserved in the int array remcomStack.
 *
 *************
 *
 *    The following gdb commands are supported:
 *
 * command          function                               Return value
 *
 *    g             return the value of the CPU registers  hex data or ENN
 *    G             set the value of the CPU registers     OK or ENN
 *
 *    mAA..AA,LLLL  Read LLLL bytes at address AA..AA      hex data or ENN
 *    MAA..AA,LLLL: Write LLLL bytes at address AA.AA      OK or ENN
 *
 *    c             Resume at current address              SNN   ( signal NN)
 *    cAA..AA       Continue at address AA..AA             SNN
 *
 *    s             Step one instruction                   SNN
 *    sAA..AA       Step one instruction from AA..AA       SNN
 *
 *    k             kill
 *
 *    ?             What was the last sigval ?             SNN   (signal NN)
 *
 * All commands and responses are sent with a packet which includes a
 * checksum.  A packet consists of
 *
 * $<packet info>#<checksum>.
 *
 * where
 * <packet info> :: <characters representing the command or response>
 * <checksum>    :: < two hex digits computed as modulo 256 sum of <packetinfo>>
 *
 * When a packet is received, it is first acknowledged with either '+' or '-'.
 * '+' indicates a successful transfer.  '-' indicates a failed transfer.
 *
 * Example:
 *
 * Host:                  Reply:
 * $m0,10#2a               +$00010203040506070809101112131415#42
 *
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <err.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <linux/kvm.h>

#include "ukvm.h"
#include "ukvm_hv_kvm.h"

static int use_gdb;

static int listen_socket_fd;
static int socket_fd;
static int stepping;

#define MAX_BREAKPOINTS    8
static uint64_t breakpoints[MAX_BREAKPOINTS];


static void wait_for_connect(int portn)
{
    struct sockaddr_in sockaddr;
    socklen_t sockaddr_len;
    struct protoent *protoent;
    int r;
    int opt;

    printf("GDB trying to get a connection at port %d\n", portn);

    listen_socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listen_socket_fd != -1);

    /* Allow rapid reuse of this port */
    opt = 1;
    r = setsockopt(listen_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt,
                   sizeof(opt));
    if (r == -1)
        perror("setsockopt(SO_REUSEADDR) failed");

    memset(&sockaddr, '\000', sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(portn);
    sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    r = bind(listen_socket_fd, (struct sockaddr *) &sockaddr,
             sizeof(sockaddr));
    if (r == -1)
        perror("Failed to bind socket");

    r = listen(listen_socket_fd, 0);
    if (r == -1)
        perror("Failed to listen on socket");

    sockaddr_len = sizeof(sockaddr);
    socket_fd = accept(listen_socket_fd, (struct sockaddr *) &sockaddr,
                       &sockaddr_len);
    if (socket_fd == -1)
        perror("Failed to accept on socket");

    close(listen_socket_fd);

    protoent = getprotobyname("tcp");
    if (!protoent) {
        perror("getprotobyname (\"tcp\") failed");
        return;
    }

    /* Disable Nagle - allow small packets to be sent without delay. */
    opt = 1;
    r = setsockopt(socket_fd, protoent->p_proto, TCP_NODELAY, &opt,
                   sizeof(opt));
    if (r == -1)
        perror("setsockopt(TCP_NODELAY) failed");

    int ip = sockaddr.sin_addr.s_addr;

    printf("GDB Connected to %d.%d.%d.%d\n", ip & 0xff, (ip >> 8) & 0xff,
           (ip >> 16) & 0xff, (ip >> 24) & 0xff);
}


static char buf[4096], *bufptr = buf;
static void flush_debug_buffer(void)
{
    char *p = buf;

    while (p != bufptr) {
        int n;

        n = send(socket_fd, p, bufptr - p, 0);
        if (n == -1) {
            perror("error on debug socket: %m");
            break;
        }
        p += n;
    }
    bufptr = buf;
}


void putDebugChar(int ch)
{
    if (bufptr == buf + sizeof(buf))
        flush_debug_buffer();
    *bufptr++ = ch;
}


int getDebugChar(void)
{
    char ch;

    recv(socket_fd, &ch, 1, 0);

    return ch;
}


/************************************************************************/
/* BUFMAX defines the maximum number of characters in inbound/outbound buffers*/
/* at least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX (400 * 4)

int remote_debug;
/*  debug >  0 prints ill-formed commands in valid packets & checksum errors */

static const char hexchars[] = "0123456789abcdef";

/* Number of registers.  */
#define NUMREGS        32

/* Number of bytes of registers.  */
#define NUMREGBYTES (NUMREGS * 8)

/* list is here: gdb/amd64-linux-nat.c */
enum regnames {
    RAX, RBX, RCX, RDX,
    RSI, RDI, RBP, RSP,
    R8, R9, R10, R11,
    R12, R13, R14, R15,
    RIP, EFLAGS, CS, SS,
    DS, ES, FS, GS
};

/*
 * these should not be static cuz they can be used outside this module
 */
long registers[NUMREGS];

/***************************  ASSEMBLY CODE MACROS *************************/
/*                                                                            */


int hex(char ch)
{
    if ((ch >= 'a') && (ch <= 'f'))
        return (ch - 'a' + 10);
    if ((ch >= '0') && (ch <= '9'))
        return (ch - '0');
    if ((ch >= 'A') && (ch <= 'F'))
        return (ch - 'A' + 10);
    return -1;
}


static unsigned char remcomInBuffer[BUFMAX];


/* scan for the sequence $<data>#<checksum>     */

unsigned char *getpacket(void)
{
    unsigned char *buffer = &remcomInBuffer[0];
    unsigned char checksum;
    unsigned char xmitcsum;
    int count;
    char ch;

    while (1) {
        /* wait around for the start character, ignore all other characters */
        do {
            ch = getDebugChar();
        } while (ch != '$');


retry:
        checksum = 0;
        xmitcsum = -1;
        count = 0;

        /* now, read until a # or end of buffer is found */
        while (count < BUFMAX - 1) {
            ch = getDebugChar();
            if (ch == '$')
                goto retry;
            if (ch == '#')
                break;
            checksum = checksum + ch;
            buffer[count] = ch;
            count = count + 1;
        }
        buffer[count] = 0;

        if (ch == '#') {
            ch = getDebugChar();
            xmitcsum = hex(ch) << 4;
            ch = getDebugChar();
            xmitcsum += hex(ch);

            if (checksum != xmitcsum) {
                if (remote_debug) {
                    fprintf(stderr,
                            "bad checksum.  My count = 0x%x, sent=0x%x. buf=%s\n",
                            checksum, xmitcsum, buffer);
                }
                putDebugChar('-');        /* failed checksum */
            } else {
                putDebugChar('+');        /* successful transfer */

                /* if a sequence char is present, reply the sequence ID */
                if (buffer[2] == ':') {
                    putDebugChar(buffer[0]);
                    putDebugChar(buffer[1]);

                    return &buffer[3];
                }

                return &buffer[0];
            }
        }
    }
}

/* send the packet in buffer.  */

void putpacket(char *buffer)
{
    unsigned char checksum;
    int count;
    char ch;

    /*  $<packet info>#<checksum>.  */
    do {
        putDebugChar('$');
        checksum = 0;
        count = 0;

        ch = buffer[count];
        while (ch) {
            putDebugChar(ch);
            checksum += ch;
            count += 1;
            ch = buffer[count];
        }

        putDebugChar('#');
        putDebugChar(hexchars[checksum >> 4]);
        putDebugChar(hexchars[checksum % 16]);
        flush_debug_buffer();
    } while (getDebugChar() != '+');
}

void debug_error(char *format, char *parm)
{
    if (remote_debug)
        fprintf(stderr, format, parm);
}


/* Indicate to caller of mem2hex or hex2mem that there has been an error. */
static volatile int mem_err;

void set_mem_err(void)
{
    mem_err = 1;
}

/* These are separate functions so that they are so short and sweet
 * that the compiler won't save any registers (if there is a fault to
 * mem_fault, they won't get restored, so there better not be any
 * saved).
 */
int get_char(char *addr)
{
    return *addr;
}


void set_char(char *addr, int val)
{
    *addr = val;
}


char *mem2hex(char *mem, char *buf, int count)
{
    int i;
    unsigned char ch;

    for (i = 0; i < count; i++) {
        ch = get_char(mem++);
        *buf++ = hexchars[ch >> 4];
        *buf++ = hexchars[ch % 16];
    }
    *buf = 0;
    return buf;
}


/* convert the hex array pointed to by buf into binary to be placed in mem */
/* return a pointer to the character AFTER the last byte written */
char *hex2mem(char *buf, char *mem, int count)
{
    int i;
    unsigned char ch;

    for (i = 0; i < count; i++) {
        ch = hex(*buf++) << 4;
        ch = ch + hex(*buf++);
        set_char(mem++, ch);
    }
    return mem;
}


int gdb_is_pc_breakpointing(uint64_t addr)
{
    int i;

    if (stepping)
        return 1;

    for (i = 0; i < MAX_BREAKPOINTS; i++) {
        if (addr == breakpoints[i])
            return 1;
    }
    return 0;
}


int gdb_insert_breakpoint(uint64_t addr)
{
    int i;

    for (i = 0; i < MAX_BREAKPOINTS; i++) {
        if (breakpoints[i] == 0) {
            breakpoints[i] = addr;
            return 1;
        }
    }
    return 0;
}


int gdb_remove_breakpoint(uint64_t addr)
{
    int i;

    for (i = 0; i < MAX_BREAKPOINTS; i++) {
        if (addr == breakpoints[i])
            breakpoints[i] = 0;
    }
    return 0;
}


void gdb_handle_exception(uint8_t *mem, size_t mem_size, int vcpufd, int sig)
{
    unsigned char *buffer;
    char obuf[4096];
    int ne = 0;

    if (sig != 0) {
        snprintf(obuf, sizeof(obuf), "S%02x", 5);
        putpacket(obuf);
    }

    while (ne == 0) {
        buffer = getpacket();

        switch (buffer[0]) {
        case 's': {
            stepping = 1;
            return;
        }
        case 'c': {
            /* Disable stepping for the next instruction */
            stepping = 0;
            return; /* Continue with program */
        }
        case 'M': {
            putpacket("OK");
            break;
        }
        case 'm': {
            uint64_t addr;
            int len;
            char *ebuf;

            addr = strtoull((char *)&buffer[1], &ebuf, 16);
            len = strtoul(ebuf + 1, NULL, 16);

            if ((addr + len) >= mem_size)
                memset(obuf, '0', len);
            else
                mem2hex((char *)mem + addr, obuf, len);
            putpacket(obuf);
            break;
        }
        case 'P': {
            putpacket("OK");
            break;
        }
        case 'g': {
            struct kvm_regs regs;
            int ret;

            ret = ioctl(vcpufd, KVM_GET_REGS, &regs);
            if (ret == -1)
                err(1, "KVM_GET_REGS");

            registers[RAX] = regs.rax;
            registers[RBX] = regs.rbx;
            registers[RCX] = regs.rcx;
            registers[RDX] = regs.rdx;

            registers[RSI] = regs.rsi;
            registers[RDI] = regs.rdi;
            registers[RBP] = regs.rbp;
            registers[RSP] = regs.rsp;

            registers[R8] = regs.r8;
            registers[R9] = regs.r9;
            registers[R10] = regs.r10;
            registers[R11] = regs.r11;
            registers[R12] = regs.r12;
            registers[R13] = regs.r13;
            registers[R14] = regs.r14;
            registers[R15] = regs.r15;

            registers[RIP] = regs.rip;
            registers[EFLAGS] = regs.rflags;

            /* TODO what about others like cs and ss? */

            mem2hex((char *) registers, obuf, NUMREGBYTES);

            putpacket(obuf);
            break;
        }
        case '?': {
            sprintf(obuf, "S%02x", SIGTRAP);
            putpacket(obuf);
            break;
        }
        case 'H': {
            putpacket("OK");
            break;
        }
        case 'q': {
            /* not supported */
            putpacket("");
            break;
        }
        case 'Z': {
            /* insert a breakpoint */
            char *ebuf;
            uint64_t addr;
            uint64_t type __attribute__((__unused__));
            uint64_t len __attribute__((__unused__));

            type = strtoull((char *)buffer + 1, &ebuf, 16);
            addr = strtoull(ebuf + 1, &ebuf, 16);
            len = strtoull(ebuf + 1, &ebuf, 16);

            gdb_insert_breakpoint(addr);
            putpacket("OK");
            break;
        }
        case 'z': {
            /* remove a breakpoint */
            char *ebuf;
            uint64_t addr;
            uint64_t type __attribute__((__unused__));
            uint64_t len __attribute__((__unused__));

            type = strtoull((char *)buffer + 1, &ebuf, 16);
            addr = strtoull(ebuf + 1, &ebuf, 16);
            len = strtoull(ebuf + 1, &ebuf, 16);

            gdb_remove_breakpoint(addr);
            putpacket("OK");
            break;
        }
        case 'k': {
            printf("Debugger asked us to quit\n");
            exit(1);
        }
        case 'D': {
            printf("Debugger detached\n");
            putpacket("OK");
            return;
        }
        default:
            putpacket("");
            break;
        }
    }

    return;
}

static void gdb_stub_start(struct ukvm_hv *hv)
{
    int i;

    for (i = 0; i < MAX_BREAKPOINTS; i++)
        breakpoints[i] = 0;

    wait_for_connect(1234);
    gdb_handle_exception(hv->mem, hv->mem_size, hv->b->vcpufd, 0);
}



static int handle_exit(struct ukvm_hv *hv)
{
    struct kvm_debug_exit_arch *arch_info;

    if (hv->b->vcpurun->exit_reason != KVM_EXIT_DEBUG)
        return -1;

    arch_info = &hv->b->vcpurun->debug.arch;
    if (gdb_is_pc_breakpointing(arch_info->pc))
        gdb_handle_exception(hv->mem, hv->mem_size, hv->b->vcpufd, 1);

    return 0;
}

static int setup(struct ukvm_hv *hv)
{
    if (!use_gdb)
        return 0;

    /* TODO check if we have the KVM_CAP_SET_GUEST_DEBUG capbility */
    struct kvm_guest_debug debug = {
        .control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_SINGLESTEP,
    };

    if (ioctl(hv->b->vcpufd, KVM_SET_GUEST_DEBUG, &debug) == -1)
        err(1, "KVM_SET_GUEST_DEBUG failed");

    assert(ukvm_core_register_vmexit(handle_exit) == 0);
    gdb_stub_start(hv);

    return 0;
}

static int handle_cmdarg(char *cmdarg)
{
    if (strncmp("--gdb", cmdarg, 5))
        return -1;

    use_gdb = 1;

    return 0;
}

static char *usage(void)
{
    return "--gdb (optional flag for running in a gdb debug session)";
}

#else /* !x86_64 */
#include "ukvm.h"

static int setup(struct ukvm_hv *hv)
{
    return 0;
}

static int handle_cmdarg(char *cmdarg)
{
    return -1;
}

static char *usage(void)
{
    return "--gdb (unsupported on your architecture)";
}
#endif

struct ukvm_module ukvm_module_gdb = {
    .name = "gdb",
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage
};
