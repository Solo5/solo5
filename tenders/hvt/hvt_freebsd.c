/*
 * Copyright (c) 2015-2018 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a sandboxed execution environment.
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
 * hvt_freebsd.c: Architecture-independent part of FreeBSD vmm(4) backend
 * implementation.
 */

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define _WITH_DPRINTF
#include <stdio.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <machine/vmm.h>
#include <sys/param.h>
#include <sys/cpuset.h>
#include <machine/vmm_dev.h>

#include "hvt.h"
#include "hvt_freebsd.h"

static struct hvt *cleanup_hvt;

static void cleanup_vmfd(void)
{
    if (cleanup_hvt != NULL && cleanup_hvt->b->vmfd != -1) {
        close(cleanup_hvt->b->vmfd);
        cleanup_hvt->b->vmfd = -1;
    }
}

struct hvt *hvt_init(size_t mem_size)
{
    int ret;

    struct hvt *hvt = malloc(sizeof (struct hvt));
    if (hvt == NULL)
        err(1, "malloc");
    memset(hvt, 0, sizeof (struct hvt));
    struct hvt_b *hvb = malloc(sizeof (struct hvt_b));
    if (hvb == NULL)
        err(1, "malloc");
    memset(hvb, 0, sizeof (struct hvt_b));
    hvt->b = hvb;
    hvb->vmfd = -1;

    int namelen = asprintf(&hvb->vmname, "solo5-%d", getpid());
    if (namelen == -1)
        err(1, "asprintf");

    char *vmmdevname;
    namelen = asprintf(&vmmdevname, "/dev/vmm/%s", hvb->vmname);
    if (namelen == -1)
        err(1, "asprintf");

    /*
     * The following code is responsible for cleaning up a VM which may have
     * been leaked by a previous instance of solo5-hvt, whose PID has now been
     * recycled, thus we have a conflict.
     *
     * We go to all this trouble due to the vmm API design being totally
     * broken and treating a VM as a "global resource", which does not allow us
     * to clean up after ourselves once we have dropped privileges in
     * hvt_drop_privileges().
     *
     * Note that this only solves the case described in the first paragraph.
     * Notably, the *current* solo5-hvt process will ALWAYS leak its VM on
     * exit. If you want an immediate workaround for that, always run "bhyvectl
     * destroy" after solo5-hvt has exited.
     *
     * Also note that this code assumes that Solo5 "owns" the /dev/vmm/solo5-%d
     * "namespace".
     */
    int tmpvmfd;
    tmpvmfd = open(vmmdevname, O_NONBLOCK | O_EXLOCK | O_RDWR, 0);

    /*
     * Case 1: Exclusive lock failed, so the previous solo5-hvt is still
     * running.
     */
    if (tmpvmfd == -1 && errno == EAGAIN)
      errx(1, "Cannot create VM '%s': Solo5 instance still running",
              hvb->vmname);

    /*
     * Case 2: VM does not exist, we can proceed with creating it for real.
     */
    else if (tmpvmfd == -1 && errno == ENOENT)
      ;

    /*
     * Case 3: Something else went wrong (e.g. we are not root).
     */
    else if (tmpvmfd == -1)
      err(1, "Cannot destroy leaked VM '%s': open()", hvb->vmname);

    /*
     * Case 4: We have a lock: This means the VM was leaked, and its solo5-hvt
     * is no longer around. Destroy it so that we can re-use the name.
     */
    else if (tmpvmfd > 0) {
      ret = sysctlbyname("hw.vmm.destroy", NULL, NULL, hvb->vmname,
                         strlen(hvb->vmname));
      if (ret == -1)
        err(1, "Cannot destroy leaked VM '%s'", hvb->vmname);
      /*
       * Experimental observation shows that close() on the vmfd after it has
       * been destroyed always(?) returns ENXIO. Test for this and warn if it
       * does NOT happen, as that may expose a change of the behaviour/a race
       * on the vmm API side.
       */
      ret = close(tmpvmfd);
      if (ret == 0)
        warnx("warning: While destroying VM '%s': close() succeded",
                hvb->vmname);
      else if (ret == -1 && errno != ENXIO)
        warn("warning: While destroying VM '%s': close()", hvb->vmname);
    }

    /*
     * At this point we have destroyed the leaked VM, if any, and know that
     * there is no other solo5-hvt using this VM name. Create the VM for real
     * and obtain an exclusive lock on its file descriptor while opening it,
     * thus allowing the above code^H^H^H^Hmess to do its job safely.
     */
    ret = sysctlbyname("hw.vmm.create", NULL, NULL, hvb->vmname, namelen);
    if (ret == -1)
        err(1, "Cannot create VM '%s'", hvb->vmname);
    cleanup_hvt = hvt;

    hvb->vmfd = open(vmmdevname, O_NONBLOCK | O_EXLOCK | O_RDWR, 0);
    if (hvb->vmfd == -1)
        err(1, "open(%s)", vmmdevname);
    atexit(cleanup_vmfd);

    struct vm_capability vmcap = {
        .cpuid = 0, .captype = VM_CAP_HALT_EXIT, .capval = 1
    };
    ret = ioctl(hvb->vmfd, VM_SET_CAPABILITY, &vmcap);
    if (ret == -1)
        err(1, "set VM_CAP_HALT_EXIT");

    struct vm_memseg memseg = {
        .segid = 0, .len = mem_size
    };
    ret = ioctl(hvb->vmfd, VM_ALLOC_MEMSEG, &memseg);
    if (ret == -1)
        err(1, "VM_ALLOC_MEMSEG");

    struct vm_memmap memmap = {
        .gpa = 0, .len = mem_size, .segid = 0, .segoff = 0,
        .prot = PROT_READ | PROT_WRITE | PROT_EXEC, .flags = 0
    };
    ret = ioctl(hvb->vmfd, VM_MMAP_MEMSEG, &memmap);
    if (ret == -1)
        err(1, "VM_MMAP_MEMSEG");

    hvt->mem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED,
            hvb->vmfd, 0);
    if (hvt->mem == MAP_FAILED)
        err(1, "mmap");
    hvt->mem_size = mem_size;
    return hvt;
}

#if HVT_DROP_PRIVILEGES
void hvt_drop_privileges()
{
    struct passwd *pw = getpwnam(VMM_USER);
    if (pw == NULL)
        err(1, "can't get %s user", VMM_USER);
    uid_t uid = pw->pw_uid;
    gid_t gid = pw->pw_gid;

    if (chroot(VMM_CHROOT) == -1)
        err(1, "chroot(%s)", VMM_CHROOT);

    if (chdir("/") == -1)
        err(1, "chdir(/)");

    if (setgroups(1, &gid) ||
            setresgid(gid, gid, gid) ||
            setresuid(uid, uid, uid))
        err(1, "unable to revoke privs");
}
#endif
