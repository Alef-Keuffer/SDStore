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
  {
    transformation_t t = transformation_str_to_enum__ (command);
    if (t == -1 || !(t == PROC_FILE || t == STATUS)) {
        char error_message[256];
        int error_message_size = snprintf (error_message, 256,"(ERROR) Unknown command: %s\n", command);
        wwrite (STDOUT_FILENO,error_message,error_message_size);
        _exit(EXIT_FAILURE);
    }
  }
  delimcat (message, command);

  if (argc > 2)
    {
      // e.g.: ./sdstore proc-file <priority> samples/file-a outputs/file-a-output bcompress nop gcompress encrypt no
      ++i;
      char *priority;
      if (!strcmp(argv[i],"-p"))
        priority  = argv[++i];
      else if (atoi(argv[i]))
        priority  = argv[i];
      else
        {
          priority = "0";
          --i;
        }

      const int pri_int = atoi(priority);
      if (!(pri_int >= 0 && pri_int <=5)) {
        wwrite(STDOUT_FILENO,"(ERROR) priority must be between 0 and 5\n",41);
        _exit(EXIT_FAILURE);
      }

      const char *src = argv[++i];
      if ((access (src, F_OK) != 0))
        {
          wwrite(STDOUT_FILENO,"(ERROR): Could not find src file\n",32);
          _exit (EXIT_SUCCESS);
        }
      const char *dst = argv[++i];
      if ((access (dst, F_OK) != 0))
        {
          wwrite(STDOUT_FILENO,"(ERROR) Could not find dst file\n",32);
          _exit (EXIT_SUCCESS);
        }
      const int transformation_start_index = ++i;
      char num_transformations[16];
      snprintf (num_transformations, 16, "%d", argc - transformation_start_index);

      delimcat (message, priority);
      delimcat (message, src);
      delimcat (message, dst);
      delimcat (message, num_transformations);
      int j = transformation_start_index;
      for (; j < argc; ++j) {
        // transformation_str_to_enum used to cause failure in case transformation does not exist
        transformation_t t = transformation_str_to_enum__(argv[j]);
        if (t == -1 || t == PROC_FILE || t == STATUS)
          {
            char error_message[256];
            int error_message_size = snprintf (error_message, 256,"(ERROR) Unknown transformation: %s\n", argv[j]);
            wwrite (STDOUT_FILENO,error_message,error_message_size);
            _exit(EXIT_FAILURE);
          }
        delimcat (message, argv[j]);
      }
    }

  mmkfifo (client_pid_str, S_IRWXU);
  fprintf (stderr, "[%ld] Created fifo with name: %s\n", (long) getpid (), client_pid_str);

  const size_t message_len = strlen (message) + 1;
  fprintf (stderr, "[%ld] Message (size=%lu) to send is: %s\n\n", (long) getpid (), message_len, message);

  if ((access (SERVER, F_OK) != 0))
    {
      wwrite(STDOUT_FILENO,"ERROR: Could not find server fifo, try again later\n",51);
      _exit (EXIT_SUCCESS);
    }

  const int fd = oopen (SERVER, O_WRONLY);
  wwrite (fd, message, message_len);
  cclose (fd);
  fprintf (stderr, "[%ld] Message sent to '%s'\n", (long) getpid (), SERVER);

  strcpy (FIFO, client_pid_str);

  char buf[BUFSIZ];
  char formatted_message[BUFSIZ];
  size_t nbytes;
  int loop = 1;
  const int in = oopen (client_pid_str, O_RDONLY);
  while (loop)
    {
      while ((nbytes = rread (in, buf, BUFSIZ)) > 0)
        {
          for (int j = 0; j < nbytes; ++j)
            if (buf[j] == EOO)
              loop = 0;
          if (!loop)
            buf[nbytes - 1] = '\0';
          const int formatted_message_size = snprintf(formatted_message,nbytes+1,"%s",buf);
          wwrite(STDOUT_FILENO,formatted_message,formatted_message_size);
        }
    }
  cclose (in);

  unlink (client_pid_str);
  fprintf (stderr, "[%ld] Unlinked %s\n", (long) getpid (), client_pid_str);
  return 0;
}

