#include <unistd.h>
#include <sys/stat.h> /*for mkfifo*/
#include <fcntl.h>

#include <stdio.h> /*for perror*/
#include <stdlib.h> /*for exit*/
#include <ctype.h> /*macros isspace,etc*/

#include "util.h"
#include "util/env.h"
#include "util/transformation.h"
#include "util/task.h"

#include "sdstored.h"

static const int NPIPE_PERMISSION = 0666;

/*!
 *
 * @param string A string that may start at a white space
 * @param i position to start reading string
 * @return first int found reading form string[i]
 */
int parse_limit (const char *string, unsigned int *i)
{
  while (string[*i] != '\0' && !isdigit(string[*i]))
    *i += 1;

  if (string[*i] == '\0')
    return -1;

  /*int assuming 4 bytes goes up to 2,147,483,647 which has 10 characters*/
  unsigned char maxLimit = 11;
  char numString[maxLimit];

  int j;
  for (j = 0; isdigit(string[*i]) && j < maxLimit; j++, *i += 1)
    numString[j] = string[*i];

  if (j > maxLimit)
    return -1;

  numString[j] = '\0';

  return atoi (numString);
}

int load_transformation_path (char *path, ENV env)
{
  /*the path is argv[3] so no need to allocate memory*/
  fprintf (stderr, "loading transformation path\n");
  env->transformations_path = path;
  fprintf (stderr, "finished transformation path\n");

}

int load_config (const char *config, ENV env)
{
  fprintf (stderr, "loading config...\n");

  const unsigned int BUF_SIZE = 8192;
  char buf[BUF_SIZE];
  int fd = oopen (config, O_RDONLY);
  ssize_t nread = rread (fd, buf, BUF_SIZE);
  fprintf (stderr, "%s\n", buf);
  cclose (fd);

  unsigned int i = 0;
  while (buf[i] != 0)
    {
      TRANSFORMATION transf = parse_transformation (buf, &i);
      unsigned int limit = parse_limit (buf, &i);
      switch (transf)
        {
      case gcompress:env->limits.gcompress = limit;
          break;
      case gdecompress:env->limits.gdecompress = limit;
          break;
      case bcompress:env->limits.bcompress = limit;
          break;
      case bdecompress:env->limits.bdecompress = limit;
          break;
      case encrypt:env->limits.encrypt = limit;
          break;
      case decrypt:env->limits.decrypt = limit;
          break;
      case nop:env->limits.nop = limit;
          break;
      default:fprintf (stderr, "Bad config file i=%d and buf[i]=%s\n", i, buf + i);
          return -1;
        }
    }

  fprintf (stderr, "finished loading config\n");
  return 0;
}

int main (int argc, char *argv[])
{
  if (argc != 3)
    return -1;

  struct env e;

  load_config (argv[1], &e);
  load_transformation_path (argv[2], &e);
  int fd = fifo_create_and_open (NPIPE_TO_SERVER, NPIPE_PERMISSION, O_RDONLY);

  const unsigned int MAX_LINE_SIZE = 8192;
  char line[MAX_LINE_SIZE];

  struct task tasks[8192];
  unsigned int ret;
  unsigned long long taskId = 0;

  while ((ret = readln (fd, line, MAX_LINE_SIZE)))
    {
      fprintf (stderr, "HEY\n");
      line[ret] = '\0';
      taskId = get_task (line, &tasks[taskId]);
      fprintf(stderr,"taskid is %llu\n",taskId);
      if (taskId != -1)
        print_task (&tasks[taskId]);
    }

  cclose (fd);
  return 0;

}