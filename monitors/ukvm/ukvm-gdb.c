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

#include "../ukvm-private.h"
#include "../ukvm-modules.h"
#include "../ukvm-cpu.h"
#include "../ukvm.h"
#include "../unikernel-monitor.h"

int platform_get_regs(struct platform *p, long *registers)
{
            struct kvm_regs regs;
            int ret;
            
            ret = ioctl(p->vcpu, KVM_GET_REGS, &regs);
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
            return 0;
}

uint64_t platform_get_rip(struct platform *p)
{
    struct kvm_run *run = (struct kvm_run *)p->priv;
    struct kvm_debug_exit_arch *arch_info;

    arch_info = &run->debug.arch;
    return arch_info->pc;
}

int platform_enable_debug(struct platform *p)
{
    /* TODO check if we have the KVM_CAP_SET_GUEST_DEBUG capbility */
    struct kvm_guest_debug debug = {
        .control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_SINGLESTEP,
    };

    if (ioctl(p->vcpu, KVM_SET_GUEST_DEBUG, &debug) < 0) {
        printf("KVM_SET_GUEST_DEBUG failed");
        return -1;
    }

    return 0;
}
