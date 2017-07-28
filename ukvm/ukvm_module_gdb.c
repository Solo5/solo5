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

/*
 * Based on binutils-gdb/gdb/stubs/i386-stub.c, which is:
 * Not copyrighted.
 */

/*
 * ukvm_module_gdb.c: Implements the GDB Remote Serial Protocol
 * https://sourceware.org/gdb/onlinedocs/gdb/Overview.html
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <err.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <stdbool.h>
#include <ctype.h>

#include "ukvm.h"
#include "ukvm_gdb.h"

#if defined(__linux__) && defined(__x86_64__)

#include "ukvm_gdb_kvm_x86_64.c"

#elif defined(__FreeBSD__) && defined(__x86_64__)

#include "ukvm_gdb_freebsd_x86_64.c"

#elif defined(__linux__) && defined(__aarch64__)

#include "ukvm_gdb_kvm_aarch64.c"

#else

#error Unsupported target

#endif 

static bool use_gdb = false;
static int socket_fd = 0;
static int portno = 1234; /* Default port number */
static const char hexchars[] = "0123456789abcdef";

#define BUFMAX                         4096
static char in_buffer[BUFMAX];
static unsigned char registers[BUFMAX];

/* The actual error code is ignored by GDB, so any number will do. */
#define GDB_ERROR_MSG                  "E01"

static int hex(unsigned char ch)
{
    if ((ch >= 'a') && (ch <= 'f'))
        return (ch - 'a' + 10);
    if ((ch >= '0') && (ch <= '9'))
        return (ch - '0');
    if ((ch >= 'A') && (ch <= 'F'))
        return (ch - 'A' + 10);
    return -1;
}

/*
 * Converts the (count) bytes of memory pointed to by mem into an hex string in
 * buf. Returns a pointer to the last char put in buf (null).
 */
static char *mem2hex(const unsigned char *mem, char *buf, size_t count)
{
    size_t i;
    unsigned char ch;

    for (i = 0; i < count; i++) {
        ch = *mem++;
        *buf++ = hexchars[ch >> 4];
        *buf++ = hexchars[ch % 16];
    }
    *buf = 0;
    return buf;
}

/*
 * Converts the hex string in buf into binary in mem.
 * Returns a pointer to the character AFTER the last byte written.
 */
static unsigned char *hex2mem(const char *buf,
                              unsigned char *mem, size_t count)
{
    size_t i;
    unsigned char ch;

    assert(strlen(buf) >= (2 * count));

    for (i = 0; i < count; i++) {
        ch = hex(*buf++) << 4;
        ch = ch + hex(*buf++);
        *mem++ = ch;
    }
    return mem;
}

static int wait_for_connect()
{
    int listen_socket_fd;
    struct sockaddr_in server_addr, client_addr;
    struct protoent *protoent;
    struct in_addr ip_addr;
    socklen_t len;
    int opt;

    listen_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket_fd == -1) {
        err(1, "Could not create socket");
        return -1;
    }
    
    opt = 1;
    if (setsockopt(listen_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt,
                   sizeof(opt)) == -1)
        err(1, "setsockopt(SO_REUSEADDR) failed");

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(portno);
     
    if (bind(listen_socket_fd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) == -1) {
        err(1, "bind failed");
        return -1;
    }
     
    if (listen(listen_socket_fd , 0) == -1) {
        err(1, "listen failed");
        return -1;
    }
     
    warnx("Waiting for a debugger. Connect to it like this:");
    warnx("\tgdb --ex=\"target remote localhost:%d\" KERNEL", portno);
     
    len = sizeof(client_addr);
    socket_fd = accept(listen_socket_fd, (struct sockaddr *)&client_addr, &len);
    if (socket_fd == -1) {
        err(1, "accept failed");
        return -1;
    }

    close(listen_socket_fd);

    protoent = getprotobyname("tcp");
    if (!protoent) {
        err(1, "getprotobyname (\"tcp\") failed");
        return -1;
    }

    opt = 1;
    if (setsockopt(socket_fd, protoent->p_proto, TCP_NODELAY, &opt,
                   sizeof(opt)) == -1)
        err(1, "setsockopt(TCP_NODELAY) failed");

    ip_addr.s_addr = client_addr.sin_addr.s_addr;
    warnx("Connection from debugger at %s", inet_ntoa(ip_addr));

    return 0;
}

static int send_char(char ch)
{
    /* TCP is already buffering, so no need to buffer here as well. */
    return send(socket_fd, &ch, 1, 0);
}

static char recv_char(void)
{
    unsigned char ch;
    int ret;

    ret = recv(socket_fd, &ch, 1, 0);
    if (ret < 0) {
        return -1;
    } else if (ret == 0) {
        /* The peer has performed an orderly shutdown (from "man recv"). */
        close(socket_fd);
        socket_fd = -1;
        return -1;
    } else {
        assert(ret == 1);
    }

    /* All GDB remote packets are encoded in ASCII. */
    assert(isascii(ch));

    return (char)ch;
}

/*
 * Scan for the sequence $<data>#<checksum>
 * Returns a null terminated string.
 */
static char *recv_packet(void)
{
    char *buffer = &in_buffer[0];
    unsigned char checksum;
    unsigned char xmitcsum;
    char ch;
    int count;

    while (1) {
        /* wait around for the start character, ignore all other characters */
        do {
            ch = recv_char();
            if (ch == -1)
                return NULL;
        } while (ch != '$');

retry:
        checksum = 0;
        xmitcsum = -1;
        count = 0;

        /* now, read until a # or end of buffer is found */
        while (count < BUFMAX - 1) {
            ch = recv_char();
            if (ch == -1)
                return NULL;
            if (ch == '$')
                goto retry;
            if (ch == '#')
                break;
            checksum = checksum + ch;
            buffer[count] = ch;
            count = count + 1;
        }
        /* Let's make this a C string. */
        buffer[count] = '\0';

        if (ch == '#') {
            ch = recv_char();
            if (ch == -1)
                return NULL;
            xmitcsum = hex(ch) << 4;
            ch = recv_char();
            if (ch == -1)
                return NULL;
            xmitcsum += hex(ch);

            if (checksum != xmitcsum) {
                warnx("Failed checksum from GDB. "
                      "My count = 0x%x, sent=0x%x. buf=%s",
                      checksum, xmitcsum, buffer);
                if (send_char('-') == -1)
                    /* Unsuccessful reply to a failed checksum */
                    err(1, "GDB: Could not send an ACK to the debugger.");
            } else {
                if (send_char('+') == -1)
                    /* Unsuccessful reply to a successful transfer */
                    err(1, "GDB: Could not send an ACK to the debugger.");

                /* if a sequence char is present, reply the sequence ID */
                if (buffer[2] == ':') {
                    send_char(buffer[0]);
                    send_char(buffer[1]);

                    return &buffer[3];
                }

                return &buffer[0];
            }
        }
    }
}

/*
 * Send packet of the form $<packet info>#<checksum> without waiting for an ACK
 * from the debugger. Only send_response
 */
static void send_packet_no_ack(char *buffer)
{
    unsigned char checksum;
    int count;
    char ch;

    /*
     * We ignore all send_char errors as we either: (1) care about sending our
     * packet and we will keep sending it until we get a good ACK from the
     * debugger, or (2) not care and just send it as a best-effort notification
     * when dying (see handle_ukvm_exit).
     */

    send_char('$');
    checksum = 0;
    count = 0;

    ch = buffer[count];
    while (ch) {
        send_char(ch);
        checksum += ch;
        count += 1;
        ch = buffer[count];
    }

    send_char('#');
    send_char(hexchars[checksum >> 4]);
    send_char(hexchars[checksum % 16]);
}

/*
 * Send a packet and wait for a successful ACK of '+' from the debugger.
 * An ACK of '-' means that we have to resend.
 */
static void send_packet(char *buffer)
{
    char ch;

    for (;;) {
        send_packet_no_ack(buffer);
        ch = recv_char();
        if (ch == -1)
            return;
        if (ch == '+')
            break;
    }
}

#define send_error_msg()   do { send_packet(GDB_ERROR_MSG); } while (0)

#define send_not_supported_msg()   do { send_packet(""); } while (0)

#define send_okay_msg()   do { send_packet("OK"); } while (0)

/*
 * This is a response to 'c' and 's'. In other words, the VM was
 * running and it stopped for some reason. This message is to tell the
 * debugger that whe stopped (and why). The argument code can take these
 * and some other values:
 *    - 'S AA' received signal AA
 *    - 'W AA' exited with return code AA
 *    - 'X AA' exited with signal AA
 * https://sourceware.org/gdb/onlinedocs/gdb/Stop-Reply-Packets.html
 */
static void send_response(char code, int sigval, bool wait_for_ack)
{
    char obuf[BUFMAX];
    snprintf(obuf, sizeof(obuf), "%c%02x", code, sigval);
    if (wait_for_ack)
        send_packet(obuf);
    else
        send_packet_no_ack(obuf);
}

static void gdb_handle_exception(struct ukvm_hv *hv, int sigval)
{
    char *packet;
    char obuf[BUFMAX];

    /* Notify the debugger of our last signal */
    send_response('S', sigval, true);

    for (;;) {
        ukvm_gpa_t addr = 0, result;
        gdb_breakpoint_type type;
        size_t len;
        int command, ret;

        packet = recv_packet();
        if (packet == NULL)
	    /* Without a packet with instructions with what to do next there is
             * really nothing we can do to recover. So, dying. */
	    errx(1, "GDB: Exiting as we could not receive the next command from "
                    "the debugger.");

        /*
         * From the GDB manual:
	 * "At a minimum, a stub is required to support the ‘g’ and ‘G’
	 * commands for register access, and the ‘m’ and ‘M’ commands
	 * for memory access. Stubs that only control single-threaded
	 * targets can implement run control with the ‘c’ (continue),
	 * and ‘s’ (step) commands."
         */
        command = packet[0];
        switch (command) {
        case 's': {
            /* Step */
            if (sscanf(packet, "s%"PRIx64, &addr) == 1) {
		/* not supported, but that's OK as GDB will retry with the
                 * slower version of this: update all registers. */
                send_not_supported_msg();
                break; /* Wait for another command. */
            }
	    if (ukvm_gdb_enable_ss(hv) == -1) {
                    send_error_msg();
                    break; /* Wait for another command. */
            }
            return; /* Continue with program */
        }

        case 'c': {
            /* Continue (and disable stepping for the next instruction) */
            if (sscanf(packet, "c%"PRIx64, &addr) == 1) {
		/* not supported, but that's OK as GDB will retry with the
                 * slower version of this: update all registers. */
                send_not_supported_msg();
                break; /* Wait for another command. */
            }
	    if (ukvm_gdb_disable_ss(hv) == -1) {
                    send_error_msg();
                    break; /* Wait for another command. */
            }
            return; /* Continue with program */
        }

        case 'm': {
            /* Read memory content */
            if (sscanf(packet, "m%"PRIx64",%zx",
                       &addr, &len) != 2) {
                send_error_msg();
                break;
            }

            if ((addr > hv->mem_size) ||
                add_overflow(addr, len, result) ||
                (result > hv->mem_size)) {
		/* Don't panic about this, just return error so the debugger
                 * tries again. */
                send_error_msg();
            } else {
                mem2hex(hv->mem + addr, obuf, len);
                send_packet(obuf);
            }
            break; /* Wait for another command. */
        }

        case 'M': {
            /* Write memory content */
            assert(strlen(packet) <= sizeof(obuf));
            if (sscanf(packet, "M%"PRIx64",%zx:%s", &addr, &len, obuf) != 3) {
                send_error_msg();
                break;
            }

            if ((addr > hv->mem_size) ||
                add_overflow(addr, len, result) ||
                (result > hv->mem_size)) {
		/* Don't panic about this, just return error so the debugger
                 * tries again. */
                send_error_msg();
            } else {
                hex2mem(obuf, hv->mem + addr, len);
                send_okay_msg();
            }
            break; /* Wait for another command. */
        }

        case 'g': {
            /* Read general registers */
            len = BUFMAX;
            if (ukvm_gdb_read_registers(hv, registers, &len) == -1) {
                send_error_msg();
            } else {
                mem2hex(registers, obuf, len);
                send_packet(obuf);
            }
            break; /* Wait for another command. */
        }

        case 'G': {
            /* Write general registers */
            len = BUFMAX;
            /* Call read_registers just to get len (not very efficient). */
            if (ukvm_gdb_read_registers(hv, registers, &len) == -1) {
                send_error_msg();
                break;
            }
            /* Packet looks like 'Gxxxxx', so we have to skip the first char */
            hex2mem(packet + 1, registers, len);
            if (ukvm_gdb_write_registers(hv, registers, len) == -1) {
                send_error_msg();
                break;
            }
            send_okay_msg();
            break; /* Wait for another command. */
        }

        case '?': {
            /* Return last signal */
            send_response('S', sigval, true);
            break; /* Wait for another command. */
        }

        case 'Z':
            /* Insert a breakpoint */
        case 'z': {
            /* Remove a breakpoint */
            packet++;
            if (sscanf(packet, "%"PRIx32",%"PRIx64",%zx",
                       &type, &addr, &len) != 3) {
                send_error_msg();
                break;
            }

            if ((addr > hv->mem_size) ||
                add_overflow(addr, len, result) ||
                (result > hv->mem_size)) {
		/* Don't panic about this, just return error so the debugger
                 * tries again. */
                send_error_msg();
                break;
            }

            if (command == 'Z')
                ret = ukvm_gdb_add_breakpoint(hv, type, addr, len);
            else
                ret = ukvm_gdb_remove_breakpoint(hv, type, addr, len);

            if (ret == -1)
                send_error_msg();
            else
                send_okay_msg();
            break;
        }

        case 'k': {
            warnx("Debugger asked us to quit");
            send_okay_msg();
            break;
        }

        case 'D': {
            warnx("Debugger detached");
            send_okay_msg();
            return;
        }

        default:
            send_not_supported_msg();
            break;
        }
    }

    return;
}

static void gdb_stub_start(struct ukvm_hv *hv)
{
    wait_for_connect();
    gdb_handle_exception(hv, GDB_SIGNAL_FIRST);
}

/*
 * Maps a VM exit to a GDB signal, and if it is of type trap (breakpoint or step),
 * we handle it here (and not in the vcpu loop). We force the handling here, by 
 * returning 0; and return -1 otherwise.
 */
static int handle_exit(struct ukvm_hv *hv)
{
    int sigval = 0;

    if (ukvm_gdb_read_last_signal(hv, &sigval) == -1)
        /* Handle this exit in the vcpu loop */
        return -1;

    switch(sigval) {
    case GDB_SIGNAL_TRAP:
        gdb_handle_exception(hv, sigval);
        return 0;

    case GDB_SIGNAL_TERM:
        /* We exited with return code 0 */
        send_response('W', 0, true);
        return -1;

    case GDB_SIGNAL_QUIT:
    case GDB_SIGNAL_KILL:
    case GDB_SIGNAL_SEGV:
        gdb_handle_exception(hv, sigval);
        return 0;

    default:
        /* Handle this exit in the vcpu loop */
        return -1;
    }
}

void handle_ukvm_exit(void)
{
    /* Tell the debugger that we exited with a "SIGKILL", and
     * don't wait for an ACK. */
    send_response('X', GDB_SIGNAL_KILL, false);
}

static int setup(struct ukvm_hv *hv)
{
    if (!use_gdb)
        return 0;

    if (ukvm_core_register_vmexit(handle_exit) == -1)
        return -1;

    if (ukvm_gdb_supported() == -1)
        errx(1, "GDB support not implemented on this backend/architecture");

    /*
     * GDB clients can change memory, and software breakpoints work by
     * replacing instructions with int3's.
     */
    if (mprotect(hv->mem, hv->mem_size,
                 PROT_READ | PROT_WRITE | PROT_EXEC) == -1)
        err(1, "GDB: Cannot remove guest memory protection");

    /* Notify the debugger that we are dying. */
    atexit(handle_ukvm_exit);

    gdb_stub_start(hv);

    return 0;
}

static int handle_cmdarg(char *cmdarg)
{
    if (!strcmp("--gdb", cmdarg)) {
        use_gdb = true;
        return 0;
    } else if (!strncmp("--gdb-port=", cmdarg, 11)) {
        int rc = sscanf(cmdarg, "--gdb-port=%d", &portno);
        if (rc != 1 || portno < 0 || portno > 65535) {
            errx(1, "Malformed argument to --gdb-port");
        }
        return 0;
    }
    return -1;
}

static char *usage(void)
{
    return "--gdb (optional flag for running in a gdb debug session)\n"
        "    [ --gdb-port=1234 ] (port to use) ";
}

struct ukvm_module ukvm_module_gdb = {
    .name = "gdb",
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage
};
