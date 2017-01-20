#ifndef _FCNTL_H
#define _FCNTL_H

int fcntl(int, int, ...);
int open(const char *, int, ...);
#define O_RDONLY (1<<0)
#define O_WRONLY (1<<1)
#define O_APPEND (1<<2)
#define O_CREAT (1<<3)
#define O_TRUNC (1<<4)
#define O_EXCL (1<<5)

#endif
