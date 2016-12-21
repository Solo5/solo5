#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define STUB_ABORT(function) \
    int __unsup_##function(void) __asm__(#function) __attribute__((noreturn)); \
    int __unsup_##function(void) \
    { \
        printf("STUB: abort: %s() called\n", #function); \
	abort(); \
    }

#define STUB_WARN_ONCE(type, function, ret) \
    type __unsup_##function(void) __asm__(#function); \
    type __unsup_##function(void) \
    { \
        static int called = 0; \
        if (!called) {\
            printf("STUB: %s() called\n", #function); \
            called = 1; \
        } \
	errno = ENOSYS; \
	return ret; \
    }

#define STUB_IGNORE(type, function, ret) \
    type __unsup_##function(void) __asm__(#function); \
    type __unsup_##function(void) \
    { \
	errno = ENOSYS; \
	return ret; \
    }

/* stdio.h */
STUB_WARN_ONCE(int, fflush, 0);
STUB_ABORT(rename);
STUB_ABORT(sscanf); /* Used only for parsing OCAMLRUNPARAM, never called */
/*
 * The following stubs are not required by the OCaml runtime, but are
 * needed to build the freestanding version of GMP used by Mirage.
 */
STUB_WARN_ONCE(int, fread, 0);
STUB_WARN_ONCE(int, getc, EOF);
STUB_WARN_ONCE(int, ungetc, EOF);
STUB_WARN_ONCE(int, fwrite, 0);
STUB_WARN_ONCE(int, fputc, EOF);
STUB_WARN_ONCE(int, putc, EOF);
STUB_WARN_ONCE(int, ferror, 1);

/* stdlib.h */
STUB_WARN_ONCE(char *, getenv, NULL);
STUB_ABORT(system);

/* unistd.h */
STUB_WARN_ONCE(int, chdir, -1);
STUB_ABORT(close);
STUB_ABORT(getcwd);
STUB_WARN_ONCE(pid_t, getpid, 2);
STUB_WARN_ONCE(pid_t, getppid, 1);
STUB_IGNORE(int, isatty, 0);
STUB_IGNORE(off_t, lseek, -1);
STUB_ABORT(read);
STUB_IGNORE(int, readlink, -1);
STUB_ABORT(unlink);

/* dirent.h */
STUB_WARN_ONCE(int, closedir, -1);
STUB_WARN_ONCE(void *, opendir, NULL);
STUB_WARN_ONCE(struct dirent *, readdir, NULL);

/* fcntl.h */
STUB_ABORT(fcntl);
STUB_WARN_ONCE(int, open, -1);

/* signal.h */
STUB_IGNORE(int, sigaction, -1);
STUB_IGNORE(int, sigsetjmp, 0);
STUB_IGNORE(int, sigaddset, -1);
STUB_IGNORE(int, sigdelset, -1);
STUB_IGNORE(int, sigemptyset, -1);
STUB_IGNORE(int, sigprocmask, -1);
/*
 * The following stubs are not required by the OCaml runtime, but are
 * needed to build the freestanding version of GMP used by Mirage.
 */
STUB_ABORT(raise);

/* string.h */
STUB_ABORT(strerror);

/* sys/stat.h */
STUB_WARN_ONCE(int, stat, -1);
