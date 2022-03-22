#include <unistd.h>
#include <sys/stat.h> /*for mkfifo*/
#include <fcntl.h>
#include <stdio.h> /*for perror*/
#include <stdlib.h> /*for exit*/
#include <errno.h>
#include <ctype.h>

#include "util.h"
#include "env.h"

/*! @addtogroup transformations
 * @{ */
void transformation_apply (const char *transf, const char *src, const char *dst, const char* transformation_path)
{
#include <string.h>

  char file[strlen(transformation_path)];
  strcpy (file,transformation_path);
  strcat (file, transf);

  pid_t pid;
  if ((pid = fork ()) == 0)
    execlp (file, transf, src, dst, NULL);

  else if (pid < 0)
    {
      perror ("transformation failed forking");
      exit (EXIT_FAILURE);
    }
}
//! @} end of group transformations


/*!
 *
 * @param string A string that may start with whitespace
 * @param i position to start reading string
 * @return The first transformation found reading from string[i]
 */
TRANSFORMATION parse_transformation (const char *string, int *i)
{
  while (!isalpha(string[*i])) *i += 1;

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
      fprintf (stderr, "Invalid operation ending at: i=%d\n", *i);
      return -1;
    }
}

/*!
 *
 * @param string A string that may start at a white space
 * @param i position to start reading string
 * @return first int found reading form string[i]
 */
int parse_limit (const char *string, int *i)
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

typedef enum status {
  pending,
  processing,
  finished
} STATUS;

typedef struct task {
  unsigned long long task_id;
  unsigned char priority; //! 0 to 5
  const char *src;
  const char *dst;
  TRANSFORMATION *transformations;
} *TASK;

const char *FIFO_SERVER_WRITE = "fromServer";
const int FIFO_PERMISSION = 0666;

int load_transformation_path (char *path, ENV env)
{
  /*the path is argv[3] so no need to allocate memory*/
  env->transformations_path = path;
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

  int i = 0;
  while (buf[i] != 0)
    {
      TRANSFORMATION transf = parse_transformation (buf, &i);
      int limit = parse_limit (buf, &i);
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
      default:fprintf (stderr, "Bad config file i=%d and buf[i]=%s\n", i,buf+i);
          return -1;
        }
    }

  fprintf (stderr, "finished loading config");
  return 0;
}

int main (int argc, char *argv[])
{
  if (argc != 3)
    return -1;

  struct env e;

  load_config (argv[1], &e);
  load_transformation_path (argv[2], &e);

  return 0;

}