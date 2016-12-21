#ifndef _DIRENT_H
#define _DIRENT_H

typedef int DIR;
int closedir(DIR *);
DIR *opendir(const char *);
struct dirent {
    char *d_name;
};
struct dirent *readdir(DIR *);

#endif
