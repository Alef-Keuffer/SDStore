#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "util/common.h"

void action (char *ops)
{
  fprintf (stderr, "[client] heard: ");
  for (int i = 0; ops[i] != EOO; ++i)
    {
      fprintf (stderr, "%c", ops[i]);
    }
}

static char FIFO[32];
static void sig_handler (int signum)
{
  signum++; // just for no error about unused variable
  unlink (FIFO);
  _exit (0);
}

int main (int argc, char *argv[])
{
  fprintf (stderr, "argc=%d\n", argc);
  if (argc < 2)
    {
      fprintf (stderr, "Not enough arguments");
      return 0;
    }

  char ops[BUFSIZ];
  pid_t clientPid = getpid ();
  int i = 0;
  i++;
  ops[i - 1] = snprintf (ops + i, 31, "%d", clientPid) + 1;
  i += ops[i - 1];

  int k = 1;
  ops[i++] = parseOp (argv[k++]);

  switch (ops[i - 1])
    {
      case PROC_FILE:
        /*sdstore proc-file ⟨priority⟩ S ⟨src⟩ S ⟨dst⟩ S ⟨ops⟩⁺*/
        ops[i++] = atoi (argv[k++]); /*priority*/
      i++;
      ops[i - 1] = snprintf (ops + i, 255, "%s", argv[k++]) + 1; /*src*/
      fprintf (stderr, "src is %s\n", ops + i);
      i += ops[i - 1];
      i++;
      ops[i - 1] = snprintf (ops + i, 255, "%s", argv[k++]) + 1; /*dst*/
      fprintf (stderr, "dst is %s\n", ops + i);
      i += ops[i - 1];
      ops[i++] = argc - k;
      fprintf (stderr, "number of ops is %d\n", ops[i - 1]);
      fprintf (stderr, "k=%d\n", k);
      for (; k < argc; k++, i++)
        ops[i] = parseOp (argv[k]);
      break;
      case STATUS:
        fprintf (stderr, "Parsed STATUS\n");
      break;
    }
  ops[i++] = EOO;

  fprintf (stderr, "Message to send is:");
  for (int j = 0; ops[j] != EOO; j++)
    fprintf (stderr, " %d", ops[j]);
  fprintf (stderr, "\n");

  const char *CLIENT = ops + 1;
  mkfifo (CLIENT, S_IRWXU);
  fprintf (stderr, "[%ld] Created fifo with name: %s\n", (long) getpid (), CLIENT);

  speakTo (SERVER, ops);
  fprintf (stderr, "Spoke to '%s'\n", SERVER);

  strcpy (FIFO, CLIENT);
  signal (SIGINT, sig_handler);
  signal (SIGTERM, sig_handler);

  char c;

  int loop = 1;
  while (loop)
    {
      int in = open (CLIENT, O_RDONLY);
      while (read (in, &c, 1) > 0) {
        if (c != EOO)
          fprintf (stderr, "%c", c);
        if (c == EOO)
          loop = 0;
      }
      close (in);
    }

  fprintf (stderr, "Listened as %s\n", CLIENT);
  unlink (CLIENT);
  fprintf (stderr, "Unlinked %s\n", CLIENT);
  return 0;
}