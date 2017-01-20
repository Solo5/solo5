#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

static size_t sn_write(FILE *f, const char *s, size_t l)
{
	size_t k = f->wend - f->wpos;
	if (k > l) k = l;
	memcpy(f->wpos, s, k);
	f->wpos += k;
	/* pretend to succeed, but discard extra data */
	return l;
}

int vsnprintf(char *restrict s, size_t n, const char *restrict fmt, va_list ap)
{
	int r;
	char b;
	FILE f = { .write = sn_write };

	if (n-1 > INT_MAX-1) {
		if (n) {
			errno = EOVERFLOW;
			return -1;
		}
		s = &b;
		n = 1;
	}

	/* Ensure pointers don't wrap if "infinite" n is passed in */
	if (n > (char *)0+SIZE_MAX-s-1) n = (char *)0+SIZE_MAX-s-1;
	f.wpos = s;
	f.wend = (s+n);
	r = vfprintf(&f, fmt, ap);

	/* Null-terminate, overwriting last char if dest buffer is full */
	if (n) f.wpos[-(f.wpos == f.wend)] = 0;
	return r;
}
