#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/times.h>
#include <unistd.h>

#include <solo5.h>

/*
 * Global errno lives in this module.
 */
int errno;

/*
 * Standard output and error "streams".
 */
static size_t console_write(FILE *f __attribute__((unused)), const char *s,
	size_t l)
{
    return solo5_console_write(s, l);
}

static FILE console = { .write = console_write };
FILE *stderr = &console;
FILE *stdout = &console;

ssize_t write(int fd, const void *buf, size_t count)
{
    if (fd == 1 || fd == 2)
	return solo5_console_write(buf, count);
    errno = ENOSYS;
    return -1;
}

void exit(int status __attribute__((unused)))
{
    solo5_exit();
}

void abort(void)
{
    solo5_console_write("Aborted\n", 8);
    solo5_exit();
}

/*
 * Malloc family of functions directly wrap Solo5 interfaces.
 */
void *malloc(size_t size)
{
    return solo5_malloc(size);
}

void free(void *p)
{
    solo5_free(p);
}

void *calloc(size_t n, size_t size)
{
    return solo5_calloc(n, size);
}

void *realloc(void *p, size_t size)
{
    return solo5_realloc(p, size);
}

/*
 * System time.
 */
#define NSEC_PER_SEC 1000000000ULL

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    if (tv != NULL) {
	uint64_t now = solo5_clock_wall();
	tv->tv_sec = now / NSEC_PER_SEC;
	tv->tv_usec = (now % NSEC_PER_SEC) / 1000ULL;
    }
    if (tz != NULL) {
	memset(tz, 0, sizeof(*tz));
    }
    return 0;
}

clock_t times(struct tms *buf)
{
    memset(buf, 0, sizeof(*buf));
    return (clock_t)solo5_clock_monotonic();
}
