#ifndef SDSTORE_UTIL_H
#define SDSTORE_UTIL_H
int fifo_create_write(const char *path, mode_t mode);
ssize_t rread(int fd, void *buf, ssize_t nbytes);
int oopen(const char *file, int oflag);
int cclose(int fd);
#endif //SDSTORE_UTIL_H
