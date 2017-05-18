/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of ukvm, a unikernel monitor.
 *
 * Based on binutils-gdb/gdb/stubs/i386-stub.c, which is:
 * Not copyrighted.
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
 * ukvm_module_gdb.c: Implements the GDB Remote Serial Protocol
 * https://sourceware.org/gdb/onlinedocs/gdb/Overview.html
 */

#define _GNU_SOURCE
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

#include "ukvm.h"
#include "ukvm_gdb.h"

static int use_gdb = 0;
static int socket_fd = 0;
static int portno = 1234; /* Default port number */
static const char hexchars[] = "0123456789abcdef";

#define BUFMAX                         4096
static uint8_t in_buffer[BUFMAX];
static uint8_t registers[BUFMAX];

/* The actual error code is ignored by GDB, so any number will do. */
#define GDB_ERROR_MSG                  "E01"

static int hex(char ch)
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
 * Converts the memory pointed to by mem into an hex string in buf.
 * Returns a pointer to the last char put in buf (null).
 */
static char *mem2hex(char *mem, char *buf, int count)
{
    int i;
    uint8_t ch;

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
static char *hex2mem(char *buf, char *mem, int count)
{
    int i;
    uint8_t ch;

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
        perror("GDB: Could not create socket");
        return -1;
    }
    
    opt = 1;
    if (setsockopt(listen_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt,
                   sizeof(opt)) < 0)
        perror("GDB: setsockopt(SO_REUSEADDR) failed");

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(portno);
     
    if (bind(listen_socket_fd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("GDB: bind failed");
        return -1;
    }
     
    listen(listen_socket_fd , 0);
     
    puts("GDB: Waiting for a debugger...");
     
    len = sizeof(client_addr);
    socket_fd = accept(listen_socket_fd, (struct sockaddr *)&client_addr, &len);
    if (socket_fd < 0) {
        perror("GDB: accept failed");
        return -1;
    }

    close(listen_socket_fd);

    protoent = getprotobyname("tcp");
    if (!protoent) {
        perror("GDB: getprotobyname (\"tcp\") failed");
        return -1;
    }

    opt = 1;
    if (setsockopt(socket_fd, protoent->p_proto, TCP_NODELAY, &opt,
                   sizeof(opt)) < 0)
        perror("GDB: setsockopt(TCP_NODELAY) failed");

    ip_addr.s_addr = client_addr.sin_addr.s_addr;
    printf("GDB: connection from debugger at %s\n", inet_ntoa(ip_addr));

    return 0;
}

static void send_char(char ch)
{
    /* TCP is already buffering, so no need to buffer here as well. */
    send(socket_fd, &ch, 1, 0);
}

static char recv_char(void)
{
    char ch;
    recv(socket_fd, &ch, 1, 0);
    return ch;
}

/* scan for the sequence $<data>#<checksum> */
static uint8_t *recv_packet(void)
{
    uint8_t *buffer = &in_buffer[0];
    uint8_t checksum;
    uint8_t xmitcsum;
    int count;
    char ch;

    while (1) {
        /* wait around for the start character, ignore all other characters */
        do {
            ch = recv_char();
        } while (ch != '$');

retry:
        checksum = 0;
        xmitcsum = -1;
        count = 0;

        /* now, read until a # or end of buffer is found */
        while (count < BUFMAX - 1) {
            ch = recv_char();
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
            ch = recv_char();
            xmitcsum = hex(ch) << 4;
            ch = recv_char();
            xmitcsum += hex(ch);

            if (checksum != xmitcsum) {
                fprintf(stderr,
                        "bad checksum.  My count = 0x%x, sent=0x%x. buf=%s\n",
                        checksum, xmitcsum, buffer);
                send_char('-');        /* failed checksum */
            } else {
                send_char('+');        /* successful transfer */

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

static void send_packet(char *buffer)
{
    uint8_t checksum;
    int count;
    char ch;

    /*  $<packet info>#<checksum>.  */
    do {
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
    } while (recv_char() != '+');
}

static void gdb_handle_exception(struct ukvm_hv *hv, int sigval)
{
    uint8_t *packet;
    char obuf[BUFMAX];

    /* Notify the debugger of our last signal */
    snprintf(obuf, sizeof(obuf), "S%02x", sigval);
    send_packet(obuf);

    for (;;) {
        uint64_t addr = 0, result;
        uint32_t type, len;
        int command, ret;

        packet = recv_packet();

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
            if (sscanf((char *)packet, "s%"PRIx64, &addr) == 1) {
		/* not supported, but that's OK as GDB will retry with the
                 * slower version of this: update all registers. */
                send_packet("");
                break; /* Wait for another command. */
            }
	    if (ukvm_gdb_enable_ss(hv) < 0) {
                    send_packet(GDB_ERROR_MSG);
                    break; /* Wait for another command. */
            }
            return; /* Continue with program */
        }

        case 'c': {
            /* Continue (and disable stepping for the next instruction) */
            if (sscanf((char *)packet, "c%"PRIx64, &addr) == 1) {
		/* not supported, but that's OK as GDB will retry with the
                 * slower version of this: update all registers. */
                send_packet("");
                break; /* Wait for another command. */
            }
	    if (ukvm_gdb_disable_ss(hv) < 0) {
                    send_packet(GDB_ERROR_MSG);
                    break; /* Wait for another command. */
            }
            return; /* Continue with program */
        }

        case 'm': {
            /* Read memory content */
            if (sscanf((char *)packet, "m%"PRIx64",%"PRIx32,
                       &addr, &len) != 2) {
                send_packet(GDB_ERROR_MSG);
                break;
            }

            if ((addr > hv->mem_size) ||
                add_overflow(addr, len, result) ||
                (result > hv->mem_size)) {
                send_packet(GDB_ERROR_MSG);
            } else {
                mem2hex((char *)hv->mem + addr, obuf, len);
                send_packet(obuf);
            }
            break; /* Wait for another command. */
        }

        case 'M': {
            /* Write memory content */
            assert(strlen((char *)packet) <= sizeof(obuf));
            if (sscanf((char *)packet, "M%"PRIx64",%"PRIx32":%s",
                       &addr, &len, obuf) != 3) {
                send_packet(GDB_ERROR_MSG);
                break;
            }

            if ((addr > hv->mem_size) ||
                add_overflow(addr, len, result) ||
                (result > hv->mem_size)) {
                send_packet(GDB_ERROR_MSG);
            } else {
                hex2mem(obuf, (char *)hv->mem + addr, len);
                send_packet("");
            }
            break; /* Wait for another command. */
        }

        case 'g': {
            /* Read general registers */
            len = BUFMAX;
            if (ukvm_gdb_read_registers(hv, registers, (uint64_t *)&len) < 0) {
                send_packet(GDB_ERROR_MSG);
            } else {
                mem2hex((char *)registers, obuf, len);
                send_packet(obuf);
            }
            break; /* Wait for another command. */
        }

        case 'G': {
            /* Write general registers */
            len = BUFMAX;
            /* Call read_registers just to get len (not very efficient). */
            if (ukvm_gdb_read_registers(hv, registers, (uint64_t *)&len) < 0) {
                send_packet(GDB_ERROR_MSG);
                break;
            }
            /* Packet looks like 'Gxxxxx', so we have to skip the first char */
            hex2mem((char *)packet + 1, (char *)registers, len);
            if (ukvm_gdb_write_registers(hv, registers, len) < 0) {
                send_packet(GDB_ERROR_MSG);
                break;
            }
            send_packet("OK");
            break; /* Wait for another command. */
        }

        case '?': {
            /* Return last signal */
            sprintf(obuf, "S%02x", sigval);
            send_packet(obuf);
            break; /* Wait for another command. */
        }

        case 'Z':
            /* Insert a breakpoint */
        case 'z': {
            /* Remove a breakpoint */
            packet++;
            if (sscanf((char *)packet, "%"PRIx32",%"PRIx64",%"PRIx32,
                       &type, &addr, &len) != 3) {
                send_packet(GDB_ERROR_MSG);
                break;
            }

            if ((addr > hv->mem_size) ||
                add_overflow(addr, len, result) ||
                (result > hv->mem_size)) {
                send_packet(GDB_ERROR_MSG);
                break;
            }

            if (command == 'Z')
                ret = ukvm_gdb_add_breakpoint(hv, type, addr, len);
            else
                ret = ukvm_gdb_remove_breakpoint(hv, type, addr, len);

            if (ret < 0)
                send_packet(GDB_ERROR_MSG);
            else
                send_packet("OK");
            break;
        }

        case 'k': {
            printf("Debugger asked us to quit\n");
            send_packet("OK");
            break;
        }

        case 'D': {
            printf("Debugger detached\n");
            send_packet("OK");
            return;
        }

        default:
            /* An empty packet means unsupported. */
            send_packet("");
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

/* Returns 0 if we want to handle the exit here and not in the vcpu loop. */
static int handle_exit(struct ukvm_hv *hv)
{
    int sigval = 0;

    if (ukvm_gdb_read_last_signal(hv, &sigval) < 0)
        /* Handle this exit in the vcpu loop */
        return -1;

    switch(sigval) {
    case GDB_SIGNAL_TRAP:
        gdb_handle_exception(hv, sigval);
        return 0;

    case GDB_SIGNAL_TERM:
        send_packet("W00"); /* We exited normally */
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

static int setup(struct ukvm_hv *hv)
{
    if (!use_gdb)
        return 0;

    if (ukvm_core_register_vmexit(handle_exit) < 0)
        return -1;

    gdb_stub_start(hv);

    return 0;
}

static int handle_cmdarg(char *cmdarg)
{
    if (!strncmp("--gdb", cmdarg, 5)) {
        use_gdb = 1;
        return 0;
    } else if (!strncmp("--port=", cmdarg, 7)) {
        portno = strtol(cmdarg + 7, NULL, 10);
        return 0;
    }
    return -1;
}

static char *usage(void)
{
    return "--gdb (optional flag for running in a gdb debug session)\n"
        "    [ --port=1234 ] (port to use) ";
}

struct ukvm_module ukvm_module_gdb = {
    .name = "gdb",
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage
};
