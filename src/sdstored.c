#include <unistd.h>
#include <sys/stat.h> /*for mkfifo*/
#include <fcntl.h>
#include <stdio.h> /*for perror*/
#include <stdlib.h> /*for exit*/
#include <errno.h>
#include <ctype.h>
#include <string.h> /*for strcpy and strcmp*/

#include "util.h"
#include "env.h"

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
//! @} end of group transformations


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

typedef enum status {
  pending,
  processing,
  finished
} STATUS;

typedef struct task {
  pid_t client_pid;
  unsigned long long task_id;
  int priority; //! 0 to 5
  char src[100];
  char dst[100];
  TRANSFORMATION transformations[20];
} *TASK;

const char *NPIPE_TO_SERVER = "toServer";
const int NPIPE_PERMISSION = 0666;

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

long long global_taskid = 0;

enum COMMAND {
  proc_file,
  status
};

enum COMMAND parse_command (const char *line, unsigned int *i)
{
  const unsigned int BUF_SIZE = 100;
  char buf[BUF_SIZE];
  unsigned int j;
  while (isspace (line[*i])) *i += 1;
  for (j = 0; *i < BUF_SIZE && !isspace(line[*i]); j++, *i += 1)
    buf[j] = line[*i];
  buf[j] = '\0';

  if (!strcmp ("proc-file", buf))
    return proc_file;
  else if (!strcmp ("status", buf))
    return status;
  else
    return -1;
}

void get_task (const char *line, TASK task)
{
  char buf[BUFSIZ];
  unsigned int i;
  // "192 proc-file 2 filea fileacomp nop nop";

  //get client pid
  for (i = 0; isdigit(line[i]); i++)
    buf[i] = line[i];
  buf[i] = '\0';

  task->client_pid = atoi (buf);


  enum COMMAND command = parse_command (line, &i);
  fprintf(stderr,"after command string: '%s'\n",line+i);

  if (command != proc_file)
    return; //temporary

  task->task_id = global_taskid++;

  while(isspace(line[i])) i++;
  fprintf(stderr,"priority string is %c\n",line[i]);
  buf[0] = line[i++];
  buf[1] = '\0';
  task->priority = atoi(buf);
  fprintf(stderr,"priority is %d\n",task->priority);
  fprintf(stderr,"after priority string: '%s'\n",line+i);


  while(isspace(line[i])) i++;
  unsigned int j;
  for (j = 0; !isspace(line[i]); j++, i++)
    task->src[j] = line[i];
  task->src[j] = '\0';
  fprintf(stderr,"src is %s\n",task->src);

  while(isspace(line[i])) i++;
  for (j = 0; !isspace(line[i]); j++, i++)
    task->dst[j] = line[i];
  task->dst[j] = '\0';
  fprintf(stderr,"dst is %s\n",task->dst);

  for (j = 0; line[i] != '\0'; j++) {
      task->transformations[j] = parse_transformation (line, &i);
      fprintf (stderr,"read transforamtion %s\n", transformation_get_name (task->transformations[j]));
  }
  task->transformations[j] = -1;
}

void print_transformations (const TRANSFORMATION transformation[])
{
  for (unsigned int i = 0; transformation[i] >= 0; i++)
    fprintf(stderr,"%s ", transformation_get_name (transformation[i]));
}

void print_task (TASK task)
{
  fprintf (stderr, "task_id = %llu\n", task->task_id);
  fprintf (stderr, "\tclient_pid = %ld\n", (long) task->client_pid);
  fprintf (stderr, "\tpriority = %u\n", task->priority);
  fprintf (stderr, "\tsrc = %s\n", task->src);
  fprintf (stderr, "\tdst = %s\n", task->dst);
  //print_transformations (task->transformations);
}
int main (int argc, char *argv[])
{
  if (argc != 3)
    return -1;

  struct env e;

  load_config (argv[1], &e);
  load_transformation_path (argv[2], &e);
  int fd = fifo_create (NPIPE_TO_SERVER, NPIPE_PERMISSION, O_RDONLY);

  const unsigned int MAX_LINE_SIZE = 8192;
  char line[MAX_LINE_SIZE];

  struct task tasks[8192];
  unsigned int ret;

  while ((ret = readln (fd, line, MAX_LINE_SIZE)))
    {
      fprintf (stderr, "HEY\n");
      line[ret] = '\0';
      get_task (line, &tasks[global_taskid]);
      print_task (&tasks[global_taskid-1]);
    }

  cclose (fd);
  return 0;

}