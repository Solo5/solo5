#ifndef __UKVM_MODULES_H__
#define __UKVM_MODULES_H__

/* hypercall interfaces exported by modules are in ukvm.h */

struct ukvm_module {
    int (*get_fd)(void);
    int (*handle_exit)(struct kvm_run *run, int vcpufd, uint8_t *mem);
    int (*handle_cmdarg)(char *cmdarg);
    int (*setup)(int vcpufd, uint8_t *mem);
    char *(*usage)(void);
    const char *name;
};

extern struct ukvm_module ukvm_blk;
extern struct ukvm_module ukvm_net;
extern struct ukvm_module ukvm_gdb;

#endif
