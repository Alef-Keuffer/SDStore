#ifndef SDSTORE_UTIL_H
#define SDSTORE_UTIL_H
int fifo_create (const char *path, mode_t permissions, int oflag);
ssize_t rread (int fd, void *buf, ssize_t nbytes);
ssize_t readln (int fd, char buf[], ssize_t maxLength);
int oopen (const char *file, int oflag);
int cclose (int fd);
#endif //SDSTORE_UTIL_H
