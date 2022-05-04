#ifndef _TASK_H_
#define _TASK_H_
#include <fcntl.h>
#include "transformation.h"
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
  TRANSFORMATION transformations[20];
} *TASK;

void print_task (TASK task);
unsigned long long get_task (const char *line, TASK task);
void task_execute(ENV e, TASK task);
#endif //_TASK_H_
