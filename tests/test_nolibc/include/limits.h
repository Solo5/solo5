#ifndef _LIMITS_H
#define _LIMITS_H

#define INT_MAX  0x7fffffff
#define INT_MIN  (-1-0x7fffffff)
#if defined(__x86_64__)
#define LONG_MAX  0x7fffffffffffffffL
#define LLONG_MAX  0x7fffffffffffffffLL
#else
#error Unsupported architecture
#endif
#define LONG_MIN (-LONG_MAX-1)
#define LLONG_MIN (-LLONG_MAX-1)
#define ULONG_MAX (2UL*LONG_MAX+1)
#define NL_ARGMAX 9
#define UCHAR_MAX 255

#endif
