#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/wait.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "util/common.h"
#include "util/safe.h"

static char FIFO[32];
static void sig_handler (__attribute__((unused)) int signum)
{
  unlink (FIFO);
  _exit (EXIT_SUCCESS);
}

void delimcat (char *dest, const char *src)
{
  strcat (dest, "\31");
  strcat (dest, src);
}

int main (int argc, char *argv[])
{
  signal (SIGINT, sig_handler);
  signal (SIGTERM, sig_handler);

  fprintf (stderr, "[%ld] argc=%d\n", (long) getpid (), argc);
  if (argc < 2)
    {
      fprintf (stderr, "[%ld] Not enough arguments", (long) getpid ());
      return 0;
    }

  char message[BUFSIZ];
  message[0] = '\0';

  const pid_t client_pid = getpid ();
  static const int client_pid_max_str_size = 32;
  char client_pid_str[client_pid_max_str_size];
  snprintf (client_pid_str, client_pid_max_str_size, "%ld", (long) client_pid);
  strcat (message, client_pid_str);

  int i = 0;
  const char *command = argv[++i];
  delimcat (message, command);

  if (argc > 2)
    {
      // e.g.: ./sdstore proc-file <priority> samples/file-a outputs/file-a-output bcompress nop gcompress encrypt no
      const char *priority = argv[++i];
      const char *src = argv[++i];
      const char *dst = argv[++i];
      const int transformation_start_index = ++i;

      char num_transformations[16];
      snprintf (num_transformations, 16, "%d", argc - transformation_start_index);

      delimcat (message, priority);
      delimcat (message, src);
      delimcat (message, dst);
      delimcat (message, num_transformations);
      int j = transformation_start_index;
      for (; j < argc; ++j)
        delimcat (message, argv[j]);
    }

  mmkfifo (client_pid_str, S_IRWXU);
  fprintf (stderr, "[%ld] Created fifo with name: %s\n", (long) getpid (), client_pid_str);

  const size_t message_len = strlen (message) + 1;
  fprintf (stderr, "[%ld] Message (size=%lu) to send is: %s\n\n", (long) getpid (), message_len, message);

  if ((access (SERVER, F_OK) != 0))
    {
      fprintf (stderr, "Could not find server fifo, try again later\n");
      _exit (EXIT_SUCCESS);
    }


  const int fd = oopen (SERVER, O_WRONLY);
  wwrite (fd, message, message_len);
  cclose (fd);
  fprintf (stderr, "[%ld] Message sent to '%s'\n", (long) getpid (), SERVER);

  strcpy (FIFO, client_pid_str);

  char c;

  int loop = 1;
  while (loop)
    {
      const int in = oopen (client_pid_str, O_RDONLY);
      while (rread (in, &c, 1) > 0)
        {
          if (c != EOO)
            fprintf (stderr, "%c", c);
          if (c == EOO)
            loop = 0;
        }
      cclose (in);
    }

  unlink (client_pid_str);
  fprintf (stderr, "[%ld] Unlinked %s\n", (long) getpid (), client_pid_str);
  return 0;
}

