#include <stdio.h>
#include <ctype.h> /*macros isdigit,isalpha,etc*/
#include <stdlib.h> /*atoi*/
#include "task.h"
#include "command.h"

/*!
 *
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
    buf[i] = line[i];
  buf[i] = '\0';

  task->client_pid = atoi (buf);


  enum COMMAND command = parse_command (line, &i);
  fprintf(stderr,"after command string: '%s'\n",line+i);

  if (command != proc_file)
    return 0; //temporary

  task->task_id = taskid++;

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

  return task->task_id;
}

void print_task (TASK task)
{
  fprintf (stderr, "task_id = %llu\n", task->task_id);
  fprintf (stderr, "\tclient_pid = %ld\n", (long) task->client_pid);
  fprintf (stderr, "\tpriority = %u\n", task->priority);
  fprintf (stderr, "\tsrc = %s\n", task->src);
  fprintf (stderr, "\tdst = %s\n", task->dst);
  print_transformations (task->transformations);
}