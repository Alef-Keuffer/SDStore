#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/sendfile.h>

#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>

#include "safe.h"

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

int oopen (const char *file, int oflag)
{
  const int fd = open (file, oflag);
  if (fd < 0)
    {
      fprintf (stderr, "[%ld] ", (long) getpid ());
      perror ("oopen");
      fprintf (stderr, "oopen failed with file: %s\n", file);
      if (errno != EINTR)
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
  const long r = strtol (str, NULL, 10);
  if (r < 0)
    {
      perror ("sstrtol failed");
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

/*!
 * Does NOT exit on EINTR
 */
size_t rread (int fd, void *buf, size_t nbytes)
{
  const ssize_t r = read (fd, buf, nbytes);
  if (r < 0)
    {
      perror ("rread");
      if (errno != EINTR)
        _exit (EXIT_FAILURE);
    }
  return r;
}

ssize_t wwrite (int fd, const void *buf, size_t nbytes)
{
  const ssize_t r = write (fd, buf, nbytes);
  if (r < 0)
    {
      perror ("rread");
      _exit (EXIT_FAILURE);
    }
  return r;
}

void *mmalloc (size_t size)
{
  void *r = malloc (size);
  if (r == NULL && size != 0)
    {
      perror ("mmalloc");
      _exit (EXIT_FAILURE);
    }
  return r;
}

void mmkfifo (const char *path, mode_t mode)
{
  int r = mkfifo (path, mode);
  if (r)
    {
      perror ("mmkfifo");
      _exit (EXIT_FAILURE);
    }
}

ssize_t ssendfile (int out_fd, int in_fd, off_t *offset, size_t count)
{
  const ssize_t r = sendfile (out_fd, in_fd, offset, count);
  if (r < 0)
    {
      perror ("ssendfile");
      _exit (EXIT_FAILURE);
    }
  if (r < count)
    {
      fprintf (stderr, "WARNING: ssendfile got count=%ld but wrote %ld", count, r);
      _exit (EXIT_FAILURE);
    }
  return r;
}

pid_t wwaitpid(pid_t pid, int *stat_loc, int options) {
  pid_t r = waitpid (pid, stat_loc, options);
  if (r < 0) {
      perror ("wwaitpid");
      if (errno != ECHILD)
        _exit (EXIT_FAILURE);
  }
  return r;
}