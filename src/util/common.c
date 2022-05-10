#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"

const char *const SERVER = "SERVER";

transformation_t transformation_str_to_enum (const char *transformation_string)
{
  if (!strcmp ("bcompress", transformation_string))
    return BCOMPRESS;
  else if (!strcmp ("bdecompress", transformation_string))
    return BDECOMPRESS;
  else if (!strcmp ("decrypt", transformation_string))
    return DECRYPT;
  else if (!strcmp ("encrypt", transformation_string))
    return ENCRYPT;
  else if (!strcmp ("gcompress", transformation_string))
    return GCOMPRESS;
  else if (!strcmp ("gdecompress", transformation_string))
    return GDECOMPRESS;
  else if (!strcmp ("nop", transformation_string))
    return NOP;
  else if (!strcmp ("status", transformation_string))
    return STATUS;
  else if (!strcmp ("proc-file", transformation_string))
    return PROC_FILE;
  else
    {
      fprintf (stderr, "transformation_get_value: Unknown transformation %s", transformation_string);
      _exit (EXIT_FAILURE);
    }
}

const char *transformation_enum_to_str (const transformation_t t)
{
  switch (t)
    {
      case BCOMPRESS:
        return "bcompress";
      case BDECOMPRESS:
        return "bdecompress";
      case DECRYPT:
        return "decrypt";
      case ENCRYPT:
        return "encrypt";
      case GCOMPRESS:
        return "gcompress";
      case GDECOMPRESS:
        return "gdecompress";
      case NOP:
        return "nop";
      default:
        {
          fprintf (stderr, "transformation_enum_to_str: not valid transformation");
          _exit (EXIT_FAILURE);
        }
    }
}

void speakTo (const char *outFilename, char *ops)
{
  if (fork ())
    {
      wait (NULL);
      return;
    };

  int n = 0;
  while (ops[n++] != EOO);

  int out = open (outFilename, O_WRONLY);
  fprintf (stderr, "Opened speaking channel\n");
  write (out, ops, n);
  close (out);
  fprintf (stderr, "Closed speaking channel\n");

  _exit (0);
}

static char FIFO[32];
static void sig_handler (int signum)
{
  signum++; // just for no error about unused variable
  unlink (FIFO);
  _exit (0);
}

void listenAs (const char *inFilename, void (*action) (char *), int isPassive)
{
  if (fork ())
    return;

  mkfifo (inFilename, S_IRWXU);
  fprintf (stderr, "Created fifo\n");

  strcpy (FIFO, inFilename);
  signal (SIGINT, sig_handler);

  char c;
  char buf[BUFSIZ];
  size_t i = 0;

  int in;
  reopen:
  in = open (inFilename, O_RDONLY);
  fprintf (stderr, "Openend listening channel\n");

  while (read (in, &c, 1) > 0)
    {
      buf[i++] = c; //is reading message

      if (c == EOO)
        {
          action (buf);
          i = 0;
        }
    }

  close (in);
  fprintf (stderr, "Closed listening channel\n");
  if (isPassive)
    goto reopen;

  unlink (inFilename);
  fprintf (stderr, "Unlinked %s\n", inFilename);
  _exit (0);
}