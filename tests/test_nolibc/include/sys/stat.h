#ifndef _SYS_STAT_H
#define _SYS_STAT_H

struct stat {
    int st_mode;
};
#define S_IFDIR 0
#define S_IFMT 0
#define S_IFREG 0
#define S_ISREG(x) (0)
int stat(const char *, struct stat *);

#endif
