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

#include <Hypervisor/hv.h>
#include <Hypervisor/hv_vmx.h>

#include "../ukvm-private.h"
#include "../ukvm-modules.h"
#include "../ukvm-cpu.h"
#include "../ukvm.h"
#include "../unikernel-monitor.h"

uint64_t platform_get_rip(struct platform *p)
{
    int ret;
    uint64_t rip;
    
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_RIP, &rip);
    assert(ret == 0);
    return rip;
}

int platform_get_regs(struct platform *p, long *reg)
{
    int ret;
    uint64_t v;
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_RAX, &v);
    assert(ret == 0);
    reg[RAX] = v;

    ret = hv_vcpu_read_register(p->vcpu, HV_X86_RBX, &v);
    assert(ret == 0);
    reg[RBX] = v;
    
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_RCX, &v);
    assert(ret == 0);
    reg[RCX] = v;
    
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_RDX, &v);
    assert(ret == 0);
    reg[RDX] = v;
    
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_RSI, &v);
    assert(ret == 0);
    reg[RSI] = v;
    
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_RDI, &v);
    assert(ret == 0);
    reg[RDI] = v;
    
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_RBP, &v);
    assert(ret == 0);
    reg[RBP] = v;
    
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_RSP, &v);
    assert(ret == 0);
    reg[RSP] = v;
    
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_R8, &v);
    assert(ret == 0);
    reg[R8] = v;
    
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_R9, &v);
    assert(ret == 0);
    reg[R9] = v;
    
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_R10, &v);
    assert(ret == 0);
    reg[R10] = v;
    
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_R11, &v);
    assert(ret == 0);
    reg[R11] = v;
    
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_R12, &v);
    assert(ret == 0);
    reg[R12] = v;
    
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_R13, &v);
    assert(ret == 0);
    reg[R13] = v;
    
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_R14, &v);
    assert(ret == 0);
    reg[R14] = v;
    
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_R15, &v);
    assert(ret == 0);
    reg[R15] = v;
    
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_RIP, &v);
    assert(ret == 0);
    reg[RIP] = v;
    
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_RFLAGS, &v);
    assert(ret == 0);
    reg[EFLAGS] = v;
    
    return 0;
}

int platform_enable_debug(struct platform *p)
{
    int ret;
    uint64_t rflags;
    
    ret = hv_vcpu_read_register(p->vcpu, HV_X86_RFLAGS, &rflags);
    assert(ret == 0);
    ret = hv_vcpu_write_register(p->vcpu, HV_X86_RFLAGS,
                                 rflags | X86_EFLAGS_TF);
    assert(ret == 0);

    return 0;
}

