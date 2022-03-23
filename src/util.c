#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include "util.h"

int fifo_create_write (const char *path, mode_t mode)
{
  if (access (path, F_OK) == -1)
    { /*check if fifo already exists*/
      if (mkfifo (path, mode))
        {
          perror ("fifo_create_write failed at mkfifo");
          exit (EXIT_FAILURE);
        }
    }
  int fd = open (path, O_WRONLY);
  if (fd == -1)
    {
      perror ("fifo_create_write failed at opening the fifo");
      exit (EXIT_FAILURE);
    }
  return fd;
}

ssize_t rread (int fd, void *buf, ssize_t nbytes)
{
  ssize_t ret;
  ssize_t nread = 0;
  while (nbytes != 0 && (ret = read (fd, buf, nbytes)) != 0)
    {
      if (ret == -1)
        {
          if (errno == EINTR)
            continue;
          perror ("read");
          break;
        }
      nread += ret;
      nbytes -= ret;
      buf += ret;
    }
  return nread;
}

ssize_t readln (int fd, char buf[], ssize_t maxLength) {
  char c;
  ssize_t ret = 0;
  for (unsigned int i=0; i<maxLength && rread(fd,&c,1) && c != '\n'; i ++){
    buf[i] = c;
    ret ++;
  }
  return ret;
}

int oopen (const char *file, int oflag)
{
  int fd = open (file, oflag);
  if (fd == -1)
    perror ("oopen");
  return fd;
}

int cclose (int fd)
{
  int status = close (fd);
  if (status)
    perror ("cclose");
  return status;
}