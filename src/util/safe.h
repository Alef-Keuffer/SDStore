#ifndef SDSTORE_UTIL_H
#define SDSTORE_UTIL_H
#include <sys/types.h>
size_t rread (int fd, void *buf, size_t nbytes);
ssize_t wwrite(int fd, const void *buf, size_t nbytes);
void *mmalloc(size_t size);
int oopen (const char *file, int oflag);
void cclose (int fd);
void mmkfifo(const char *path, mode_t mode);
int ddup2(int fildes, int fildes2);
pid_t ffork();
void eexecl(const char* path, const char *arg,...);
int ppipe(int *pipedes);
int sstrtol (char *str);
#endif //SDSTORE_UTIL_H
