#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include "util.h"

int fifo_create_or_open (const char *path, mode_t permissions, int oflag)
{
  if (access (path, F_OK) == -1) /*check if fifo already exists*/
    {
      if (mkfifo (path, permissions))
        {
          perror ("fifo_create_write: failed at mkfifo");
          _exit (EXIT_FAILURE);
        }
    }
  int fd = open (path, oflag);
  if (fd == -1)
    {
      perror ("fifo_create_write: failed at opening the fifo");
      _exit (EXIT_FAILURE);
    }
  return fd;
}

ssize_t my_read (int fd, void *buf, size_t nbytes)
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
          _exit (1);
        }
      nread += ret;
      nbytes -= ret;
      buf += ret;
    }
  return nread;
}

ssize_t readln (int fd, char buf[], ssize_t maxLength)
{
  char c;
  ssize_t ret = 0;
  for (unsigned int i = 0; i < maxLength && my_read(fd, &c, 1) && c != '\n'; i++)
    {
      buf[i] = c;
      ret++;
    }
  return ret;
}

int oopen (const char *file, int oflag)
{
  const int fd = open (file, oflag);
  if (fd == -1)
    {
      fprintf (stderr, "[%ld] ", (long) getpid ());
      perror ("oopen");
      fprintf (stderr, "oopen failed with file: %s\n", file);
      _exit (EXIT_FAILURE);
    }
  return fd;
}

void cclose (int fd)
{
  const int status = close (fd);
  if (status)
    {
      fprintf (stderr, "[%ld] ", (long) getpid ());
      perror ("cclose");
      _exit (EXIT_FAILURE);
    }
}

int sstrtol (char *str)
{
  const int r = (int) strtol (str, NULL, 10);
  if (r <= 0)
    {
      perror ("sstrtol failed\n");
      _exit (EXIT_FAILURE);
    }
  return r;
}

pid_t ffork ()
{
  const pid_t pid = fork ();
  if (pid < 0)
    {
      perror ("ffork");
      _exit (EXIT_FAILURE);
    }
  return pid;
}

int ddup2 (int fildes, int fildes2)
{
  const int fd = dup2 (fildes, fildes2);
  if (fd < 0)
    {
      perror ("ddup2");
      _exit (EXIT_FAILURE);
    }
  return fd;
}

void eexecl (const char *path, const char *arg, ...)
{
  execl (path, arg);
  perror ("execl");
  _exit (EXIT_FAILURE);
}

int ppipe (int *pipedes)
{
  int fd = pipe (pipedes);
  if (fd < 0)
    {
      perror ("ppipe");

      _exit (EXIT_FAILURE);
    }
  return fd;
}

size_t rread (int fd, void *buf, size_t nbytes)
{
  const ssize_t r = read (fd, buf, nbytes);
  if (r < 0)
    {
      perror ("rread");
      _exit (EXIT_FAILURE);
    }
  return r;
}

size_t rread_less_than (int fd, void *buf, size_t nbytes)
{
  const ssize_t r = read (fd, buf, nbytes);
  if (r < 0)
    {
      perror ("rread");
      _exit (EXIT_FAILURE);
    }
  assert(r < nbytes);
  return r;
}

ssize_t wwrite(int fd, const void *buf, size_t nbytes)
{
  const ssize_t r = write (fd, buf, nbytes);
  if (r < 0)
    {
      perror ("rread");
      _exit (EXIT_FAILURE);
    }
  return r;
}

void *mmalloc(size_t size) {
  void *r = malloc(size);
  if (r == NULL && size != 0) {
    perror("mmalloc");
    _exit (EXIT_FAILURE);
  }
  return r;
}

void mmkfifo(const char *path, mode_t mode) {
  int r = mkfifo(path,mode);
  if (r) {
    perror ("mmkfifo");
    _exit(EXIT_FAILURE);
  }
}