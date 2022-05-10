#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>

#include "util/common.h"
#include "util/util.h"

static char FIFO[32];
static void sig_handler ()
{
  unlink (FIFO);
  _exit (0);
}

void delimcat (char *dest, const char *src)
{
  strcat (dest, "\31");
  strcat (dest, src);
}

void inter_message (const char *m0)
{
  char *m = malloc (strlen (m0) + 1);

  const char del[2] = "\31";
  char *tok;
  tok = strtok (m, del);
  fprintf (stderr, "task->client_pid_str = %s\n", tok);

  tok = strtok (NULL, del);
  fprintf (stderr, "command = %s\n", tok);

  tok = strtok (NULL, del);
  fprintf (stderr, "task->pri = %d\n", sstrtol (tok));

  tok = strtok (NULL, del);
  fprintf (stderr, "task->src = %s\n", tok);

  tok = strtok (NULL, del);
  fprintf (stderr, "task->dst = %s\n", tok);

  tok = strtok (NULL, del);
  fprintf (stderr, "task->num_ops = %d\n", sstrtol (tok));

  tok = strtok (NULL, del);
  transformation_t num_ops[NUMBER_OF_TRANSFORMATIONS] = {0};
  for (int i = 0; tok != NULL; ++i, tok = strtok (NULL, del))
    ++num_ops[transformation_str_to_enum (tok)];

  for (int i = 0; i < NUMBER_OF_TRANSFORMATIONS; ++i)
    fprintf (stderr, "task->num_ops[%d] = %d\n", i, num_ops[i]);

  free (m);
}

int main (int argc, char *argv[])
{
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

  mkfifo (client_pid_str, S_IRWXU);
  fprintf (stderr, "[%ld] Created fifo with name: %s\n", (long) getpid (), client_pid_str);

  const size_t message_len = strlen (message) + 1;
  fprintf (stderr, "[%ld] Message (size=%lu) to send is: %s\n\n", (long) getpid (), message_len, message);
  int fd = oopen (SERVER, O_WRONLY);
  wwrite (fd, message, message_len);
  cclose (fd);
  fprintf (stderr, "[%ld] Message sent to '%s'\n", (long) getpid (), SERVER);

  strcpy (FIFO, client_pid_str);
  signal (SIGINT, sig_handler);
  signal (SIGTERM, sig_handler);

  char c;

  int loop = 1;
  while (loop)
    {
      int in = oopen (client_pid_str, O_RDONLY);
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

