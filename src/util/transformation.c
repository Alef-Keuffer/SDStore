#include <fcntl.h>
#include <unistd.h>
#include <string.h> /*strlen*/
#include <stdio.h> /*perror*/
#include <stdlib.h> /*exit*/
#include <ctype.h> /*macros isdigit,isalpha,etc*/
#include <sys/wait.h>

#include "transformation.h"
#include "env.h"
#include "../util.h"

/*! @addtogroup transformations
 * @{ */
void transformation_apply (const char *transf, const char *src, const char *dst, const char *transformation_path)
{
  char file[strlen (transformation_path)];
  strcpy (file, transformation_path);
  strcat (file, transf);

  int in_fd = oopen (src, O_RDONLY);
  dup2 (in_fd, STDIN_FILENO);
  cclose (in_fd);
  int out_fd = oopen (dst, O_WRONLY);
  dup2 (in_fd, STDOUT_FILENO);
  close (out_fd);

  pid_t pid;
  if ((pid = fork ()) == 0)
    execlp (file, transf, src, dst, NULL);

  else if (pid < 0)
    {
      perror ("transformation failed forking");
      exit (EXIT_FAILURE);
    }
}
void apply_transformations(const TRANSFORMATION transformations[], const char *src, const char *dst, const char *transformation_path) {
  for (int i = 0; transformations[i] != -1; i++) {
      transformation_apply(transformation_get_name (transformations[i]),src,dst,transformation_path);
  }
}


int fork_exec(const char *file, const char *arg) {
  pid_t pid = fork();
  if (pid == 0)
    execlp (file,arg);
  else {
    int status;
    wait(&status);
    return WEXITSTATUS(status);
  }
}

int pipe_progs(const char* src, const char* dst, const int N ,char* progs[]) {

  int fds[2*N];
  for (int i = 0; i < N-1; i+=2)
    pipe(fds+i);

  pid_t pid;
  int fd;

  for (int i = 0; i < N; i++) {
    pid = fork();
    if (!pid) {
      if (i == 0) {
        fd = open(src,O_RDONLY);
        dup2(fd,STDIN_FILENO);
        close(fd);
        dup2(fds[i+1],STDOUT_FILENO);
      }
      else if (i == N - 1) {
        dup2(fds[i-1],STDIN_FILENO);
        fd = open(dst,O_WRONLY);
        dup2(fd,STDOUT_FILENO);
        close(fd);
      }
      else {
        dup2(fds[i-1],STDIN_FILENO);
        dup2(fds[i],STDOUT_FILENO);
      }
        fork_exec (progs[i],progs[i]);
    }
  }
  return 0;
}

void apply_transformations2(const TRANSFORMATION transformations[], const char *src, const char *dst, const char *transformation_path) {
  int i = 0;
  while (transformations[i] != -1) i++;

  char *progs[i];

  for (int j = 0; j < i; j++) {
    char string[200];
    strcat(string,transformation_path);
    strcat(string,"/");
    strcat(string, transformation_get_name (transformations[i]));
    progs[j] = calloc(strlen(string)+1,sizeof(char));
    strcpy(string,progs[i]);
  }
  pipe_progs (src,dst,i,progs);

  for (int j = 0; j < i; j++) free(progs[i]);
}

/*! @addtogroup string
 * @{ */

/*! @addtogroup parsing
 * @{ */

/*!
 *
 * @param string A string that may start with whitespace
 * @param i position to start reading string
 * @return The first transformation found reading from string[i]
 */
TRANSFORMATION parse_transformation (const char *string, unsigned int *i)
{
  if (i == NULL) i = 0;
  while (!isalpha(string[*i]))
    *i += 1;

  const int transformation_name_length_upper_bound = 20;
  char buf[transformation_name_length_upper_bound];
  int j;
  for (j = 0; isalpha(string[*i]) && j < transformation_name_length_upper_bound; *i += 1, j++)
    buf[j] = string[*i];

  buf[j] = '\0';

  if (!strcmp (TRANSFORMATION_NAMES.bcompress, buf))
    return bcompress;
  else if (!strcmp (TRANSFORMATION_NAMES.bdecompress, buf))
    return bdecompress;
  else if (!strcmp (TRANSFORMATION_NAMES.decrypt, buf))
    return decrypt;
  else if (!strcmp (TRANSFORMATION_NAMES.encrypt, buf))
    return encrypt;
  else if (!strcmp (TRANSFORMATION_NAMES.gcompress, buf))
    return gcompress;
  else if (!strcmp (TRANSFORMATION_NAMES.gdecompress, buf))
    return gdecompress;
  else if (!strcmp (TRANSFORMATION_NAMES.nop, buf))
    return nop;
  else
    {
      fprintf (stderr, "parse_transformation:n\n\tInvalid operation ending at: i=%d\n", *i);
      return -1;
    }
}
//! @} end of group parsing

const char *transformation_get_name (enum transformation t)
{
  switch (t)
    {
  case bcompress:return TRANSFORMATION_NAMES.bcompress;
  case bdecompress:return TRANSFORMATION_NAMES.bdecompress;
  case decrypt:return TRANSFORMATION_NAMES.decrypt;
  case encrypt:return TRANSFORMATION_NAMES.encrypt;
  case gcompress:return TRANSFORMATION_NAMES.gcompress;
  case gdecompress:return TRANSFORMATION_NAMES.gdecompress;
  case nop:return TRANSFORMATION_NAMES.nop;
  default:
    fprintf(stderr,"transformation_get_name: not valid transformation");
    return "";
    }
}

void transformation_get_totals(TRANSFORMATION t[], int totals[nTransformations]) {
  memset(totals,0,sizeof(int)*nTransformations);
  for (int i = 0; t[i] != -1; t++) totals[t[i]] ++;
}

int transformation_is_possible(ENV e, const int totals[nTransformations]) {

  for (int i = 0; i < nTransformations; i++)
    if (e->running[i] + totals[i] > e->limits[i])
      return 0;

  return 1;
}

void transformation_add_to_running(ENV e, const int totals[nTransformations]) {
  for (int i = 0; i < nTransformations; i++)
    e->running[i] += totals[i];
}

void transformation_sub_from_running(ENV e, const int totals[nTransformations]) {
  for (int i = 0; i < nTransformations; i++)
    e->running[i] -= totals[i];
}

void print_transformations (const TRANSFORMATION transformation[])
{
  for (unsigned int i = 0; transformation[i] != -1 && i < 20; i++)
    fprintf(stderr,"%s ", transformation_get_name (transformation[i]));
}
//! @} end of group string
//! @} end of group transformations