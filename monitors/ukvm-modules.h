#ifndef __UKVM_MODULES_H__
#define __UKVM_MODULES_H__

#include "unikernel-monitor.h"

/* hypercall interfaces exported by modules are in ukvm.h */

struct ukvm_module {
    int (*get_fd)(void);
    int (*handle_exit)(struct platform *p);
    int (*handle_cmdarg)(char *cmdarg);
    int (*setup)(struct platform *p);
    char *(*usage)(void);
    const char *name;
};

extern struct ukvm_module ukvm_blk;
extern struct ukvm_module ukvm_net;
extern struct ukvm_module ukvm_gdb;

#endif
