#include <unistd.h>
#include <sys/stat.h> /*for mkfifo*/
#include <fcntl.h>

#include <stdio.h> /*for perror*/
#include <stdlib.h> /*for exit*/
#include <ctype.h> /*macros isspace,etc*/
#include <string.h>
#include <sys/wait.h>

#include "util.h"
#include "sdstored.h"

const int NPIPE_PERMISSION = 0666;
unsigned int globalLimits[7];
unsigned int globalRunning[7];
char *globalTransformationsPath;
/* ------------------------------------------------------------------------------------------------------------------ */

/*! @addtogroup transformations
 * @{ */

const int nTransformations = 7;
enum transformation {
  bcompress,
  bdecompress,
  decrypt,
  encrypt,
  gcompress,
  gdecompress,
  nop
} TRANSFORMATION;

const char *transformation_get_name (enum transformation t)
{
  switch (t)
    {
  case bcompress:return "bcompress";
  case bdecompress:return "bdecompress";
  case decrypt:return "decrypt";
  case encrypt:return "encrypt";
  case gcompress:return "gcompress";
  case gdecompress:return "gdecompress";
  case nop:return "nop";
  default:
    fprintf(stderr,"transformation_get_name: not valid transformation");
    return "";
    }
}

enum transformation transformation_get_value (const char* transformation_string)
{
  if (!strcmp ("bcompress", transformation_string))
    return bcompress;
  else if (!strcmp ("bdecompress", transformation_string))
    return bdecompress;
  else if (!strcmp ("decrypt", transformation_string))
    return decrypt;
  else if (!strcmp ("encrypt", transformation_string))
    return encrypt;
  else if (!strcmp ("gcompress", transformation_string))
    return gcompress;
  else if (!strcmp ("gdecompress", transformation_string))
    return gdecompress;
  else if (!strcmp ("nop", transformation_string))
    return nop;
  else
    {
      fprintf (stderr, "transformation_get_value: Unknown transformation %s", transformation_string);
      return -1;
    }
}

/*! @addtogroup string
 * @{ */

/*! @addtogroup parsing
 * @{ */

/*!
 * @param string A string that may start with whitespace
 * @param i position to start reading string
 * @return The first transformation found reading from string[i]
 */
enum transformation parse_transformation (const char *string, unsigned int *i)
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

  return transformation_get_value (buf);
}
//! @} end of group parsing
void print_transformations (const enum transformation transformations[])
{
  for (unsigned int i = 0; transformations[i] != -1 && i < 20; i++)
    fprintf (stderr,"%s ", transformation_get_name (transformations[i]));
  fprintf(stderr,"\n");
}
//! @} end of group string
//! @} end of group transformations

/*! @addtogroup command
 * @{ */

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
//! @} end of group command

/* ------------------------------------------------------------------------------------------------------------------ */

/*! @addtogroup task
 * @{ */
typedef enum status {
  pending,
  processing,
  finished
} STATUS;

typedef struct task {
  char client_pid[32];
  unsigned long long task_id;
  int priority; //! 0 to 5
  char src[100];
  char dst[100];
  enum transformation transformations[20];
} *TASK;

/*!
 * @param line
 * @param task
 * @return @c task->task_id of task saved to @p task. -1 (max unsigned long long) if no tasks is generated from command.
 */
unsigned long long get_task (const char *line, TASK task)
{
  static unsigned long long taskid = 0;

  char buf[BUFSIZ];
  unsigned int i;
  // "192 proc-file 2 filea fileacomp nop nop";

  //get client pid
  for (i = 0; isdigit(line[i]); i++)
    task->client_pid[i] = line[i];
  task->client_pid[i] = '\0';

  enum COMMAND command = parse_command (line, &i);

  if (command != proc_file)
    return 0; //temporary

  task->task_id = taskid++;

  while(isspace(line[i])) i++;
  buf[0] = line[i++];
  buf[1] = '\0';
  task->priority = atoi(buf);
  fprintf(stderr,"priority is %d\n",task->priority);


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
      fprintf (stderr,"read transformation %s\n", transformation_get_name (task->transformations[j]));
  }
  task->transformations[j] = -1;

  return task->task_id;
}

void print_task (TASK task)
{
  fprintf (stderr, "task_id = %llu\n", task->task_id);
  fprintf (stderr, "\tclient_pid = %s\n", task->client_pid);
  fprintf (stderr, "\tpriority = %u\n", task->priority);
  fprintf (stderr, "\tsrc = %s\n", task->src);
  fprintf (stderr, "\tdst = %s\n", task->dst);
  fprintf (stderr, "transformations: ");
  print_transformations (task->transformations);

}

void task_execute(TASK task) {
  fprintf(stderr,"Starting task_execute\n");
  int fd_write = oopen(task->client_pid,O_WRONLY );

  //int fd_write = fifo_create_or_open (task->client_pid,NPIPE_PERMISSION,O_WRONLY);
  int totals[nTransformations];
  transformation_get_totals (task->transformations,totals);

  while(!transformation_is_possible (totals));

  transformation_add_to_running (totals);
  write(fd_write,"processing",10);

  fprintf(stderr,"Starting apply_transformations\n");
  apply_transformations (task->transformations,task->src,task->dst,e->transformations_path);

  transformation_sub_from_running (e,totals);
  write(fd_write,"completed",9);
  //close(fd_write);
}
//! @} end of group task

/* ------------------------------------------------------------------------------------------------------------------ */

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

int load_transformation_path (char *path)
{
  /*the path is argv[3] so no need to allocate memory*/
  fprintf (stderr, "loading transformation path\n");
  globalTransformationsPath = path;
  fprintf (stderr, "finished transformation path\n");

}

int load_config (const char *configFilename)
{
  fprintf (stderr, "load_config: loading config...\n");

  const unsigned int BUF_SIZE = 8192;
  char buf[BUF_SIZE];
  int fd = oopen (configFilename, O_RDONLY);
  ssize_t nread = rread (fd, buf, BUF_SIZE);
  fprintf (stderr, "load_config: read\n'''\n%s\n'''\n", buf);
  cclose (fd);

  unsigned int i = 0;
  while (buf[i] != 0)
    {
      enum transformation transf = parse_transformation (buf, &i);
      unsigned int limit = parse_limit (buf, &i);
      if (transf != -1)
        globalLimits[transf] = limit;
    }

  fprintf (stderr, "load_config: finished loading config\n");
  return 0;
}

int main (int argc, char *argv[])
{
  if (argc != 3)
    {
      fprintf(stderr,"argc != 3\n");
      return 1;
    }

  load_config (argv[1]);
  load_transformation_path (argv[2]);
  int fd = fifo_create_or_open (NPIPE_TO_SERVER, NPIPE_PERMISSION, O_RDONLY);

  const unsigned int MAX_LINE_SIZE = 8192;
  char line[MAX_LINE_SIZE];

  struct task tasks[8192];
  unsigned int ret;
  unsigned long long taskId = 0;

  while ((ret = readln (fd, line, MAX_LINE_SIZE)))
    {
      line[ret] = '\0';
      taskId = get_task (line, &tasks[taskId]);
      fprintf(stderr,"taskid is %llu\n",taskId);
      if (taskId != -1)
        print_task (&tasks[taskId]);
      task_execute (&tasks[taskId]);
    }

  cclose (fd);
  return 0;

}