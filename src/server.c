#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "util/common.h"

/*

S - Size (0-255) will add one to make it this range since messages of size 0 don't make sense.

S ⟨PID⟩ S ⟨src⟩ S ⟨dst⟩ S ⟨op⟩⁺

*/

int globalLimits[NUMBER_OF_TRANSFORMATIONS];
int globalRunning[NUMBER_OF_TRANSFORMATIONS] = {0};
char *TRANSFORMATIONS_FOLDER;

void exec (const char op)
{
  char command[BUFSIZ - 255];
  const char *op_name = transformation_get_name (op);
  strcpy (command, TRANSFORMATIONS_FOLDER);
  strcat (command, op_name);
  fprintf (stderr, "[%ld]: execl(%s)\n",(long) getpid (), command);
  execl (command, op_name, (char *) NULL);
  perror ("execl");
}

int pipe_progs (const char *m)
{
  /*m ≡ S ⟨src⟩ S ⟨dst⟩ S ⟨op⟩⁺*/
  const char *src = m + 1;
  fprintf (stderr, "pipe_progs: src is %s\n", src);
  const unsigned char s_src = src[-1];
  fprintf (stderr, "pipe_progs: s_src is %d\n", s_src);
  const char *dst = src + s_src + 1;
  fprintf (stderr, "pipe_progs: dst is %s\n", dst);
  const unsigned char s_dst = dst[-1];
  fprintf (stderr, "pipe_progs: s_dst is %d\n", s_dst);
  const char *ops = dst + s_dst + 1;
  const int n = (unsigned char) ops[-1];
  fprintf (stderr, "pipe_progs: number of ops is %d\n", n);

  int fd;
  int fds[2 * (n - 1)];
  

  if (n == 1)
    {
      if (fork () != 0)
        return 0;

      fprintf (stderr, "[%ld]: %s → p_0 → %s\n", (long) getpid (), src, dst);

      fd = open (src, O_RDONLY);
      dup2 (fd, STDIN_FILENO);
      close (fd);

      fd = open (dst, O_WRONLY);
      dup2 (fd, STDOUT_FILENO);
      close (fd);
      exec (ops[0]);
    }

  for (int i = 0; i < n; i++)
    {
      if (i < n - 1)
        {
          pipe (fds + 2 * i);
          fprintf (stderr, "[%ld]: (fds[%d],fds[%d])\n", (long) getpid (), 2 * i, 2 * i + 1);
        }
      if (fork () != 0) {
        int c = i%2 ? 2*(i-1) : 2*i+1; //then close previous else close next
        close(fds[c]);
        fprintf (stderr, "[%ld]: close(fds[%d])\n", (long) getpid (),c);
        continue;
      }
      if (i == 0)
        {
          close (fds[2 * i]);
          fd = open (src, O_RDONLY);
          dup2 (fd, 0);
          close (fd);
          dup2 (fds[2 * i + 1], 1);
          fprintf (stderr, "[%ld]: %s → p_0 → fds[%d]\n", (long) getpid (), src, 2 * i + 1);
        }
      else if (i == n - 1)
        {
          close (fds[2 * i - 1]);
          fd = open (dst, O_WRONLY);
          dup2 (fd, 1);
          close (fd);
          dup2 (fds[2 * (i - 1)], 0);
          fprintf (stderr, "[%ld]: fds[%d] → p_%d → %s\n", (long) getpid (), 2 * (i - 1), i, dst);
        }
      else
        {
          dup2 (fds[2 * (i - 1)], 0);
          dup2 (fds[2 * i + 1], 1);
          fprintf (stderr, "[%ld]: fds[%d] → p_%d → fds[%d]\n", (long) getpid (), 2 * (i - 1), i, 2 * i + 1);
        }
      exec (ops[i]);
    }
  return 0;
}

/*!
 * @param ops ⟨size:nat⟩ ⟨op⟩⁺
 */
int isPossible(const char *ops ) {
    int i = 0;
    const int n = ops[i++];
    char opsTotal[NUMBER_OF_TRANSFORMATIONS] = {0};

    for (int j = 0; j < n; i++, j++)
        opsTotal[ops[i]] ++;

    for (int j = 0; j < NUMBER_OF_TRANSFORMATIONS; j++) {
        if (globalRunning[j] + opsTotal[j] > globalLimits[j])
            return 0;
    }
    return 1;
}

/*
void dispatch(CLIENT,) {
    if (!fork())
        return;
    while (!isPossible());

    speakTo (CLIENT, "processing");
    pipe_progs (m + i + 1); */
/* +1 assuming proc-file comes with priority parameter. *//*

    speakTo (CLIENT, "completed\x80");
}
*/

void action (char *m)
{
  fprintf (stderr, "[server] heard:");
  for (int i = 0; m[i] != EOO; i++)
    fprintf (stderr, " %d", m[i]);
  fprintf (stderr, "\n");

  const char *CLIENT = m + 1;

  int i = (unsigned char) m[0];
  while (m[i] != EOO)
    {
      switch (m[i++])
        {
          case PROC_FILE:
            speakTo (CLIENT, "pending");
            speakTo (CLIENT, "processing");
            pipe_progs (m + i + 1); /* +1 assuming proc-file comes with priority parameter. */
            speakTo (CLIENT, "completed\x80");
          break;
          case STATUS:
            speakTo (CLIENT, "status not implemented yet\0\x80");
          break;
        }
    }
}

int main (int argc, char *argv[])
{
  (void) argc; /* unused parameter */
  fprintf (stderr, "loading config...\n");
  char buf[BUFSIZ];
  const char *configFilename = argv[1];
  int fd = open (configFilename, O_RDONLY);
  read (fd, buf, BUFSIZ);
  fprintf (stderr, "config payload:\n'''\n%s\n'''\n", buf);
  close (fd);

  int i = 0;
  while (buf[i] != 0)
    {
      int j = i;
      while (buf[i] != ' ')
        i++;
      buf[i++] = 0;
      int op = parseOp (buf + j);
      j = i;
      while (buf[i] != 0 && buf[i] != '\n')
        i++;
      buf[i++] = 0;
      globalLimits[op] = atoi (buf + j);
    }
  fprintf (stderr, "loaded config\n");
  for (int j = 0; j < NUMBER_OF_TRANSFORMATIONS; j++)
    fprintf (stderr, "globalLimits[%d] = %d\n", j, globalLimits[j]);
  TRANSFORMATIONS_FOLDER = argv[2];
  fprintf (stderr, "TRANSFORMATIONS_FOLDER = %s\n", TRANSFORMATIONS_FOLDER);

  listenAs (SERVER, action, 1);

  while (wait (NULL) > 0);

  return 0;
}

/*
S ≡ size

char** tasks = malloc(N);
e ∈ tasks
e → S ⟨client_pid⟩ ⟨proc-file⟩ ⟨priority⟩ S ⟨src⟩ S ⟨dst⟩ S ⟨ops⟩⁺ ⟨EOO⟩

priority,arrival_time ← {t ∣ t.priority     = max(priorities(tasks))
                           ∧ t.arrival_date = min({q ∈ tasks ∣ q.priority = t.priority}) }


other message for other parts of program:
S ⟨client_pid⟩ ⟨status⟩ ⟨EOO⟩

*/