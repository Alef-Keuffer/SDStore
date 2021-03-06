#ifndef SERVER_H_
#define SERVER_H_
#include <sys/types.h>
#include "util/pqueue.h"
#include "util/common.h"
typedef struct task_t {
    size_t pos; //private
    pqueue_pri_t pri;
    char *client_pid_str;
    char *src;
    char *dst;
    char *ops;
    int num_ops;
    pid_t monitor;
    char ops_totals[NUMBER_OF_TRANSFORMATIONS];
} task_t;
#endif //SERVER_H_
