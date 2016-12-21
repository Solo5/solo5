#ifndef _ENDIAN_H
#define _ENDIAN_H

#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#if defined(__x86_64__)
#define __BYTE_ORDER __LITTLE_ENDIAN
#else
#error Unsupported architecture
#endif

#endif
