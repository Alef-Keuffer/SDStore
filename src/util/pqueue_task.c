#include "pqueue.h"
#include "pqueue_task.h"
#include "../_server.h"

static int
cmp_pri(pqueue_pri_t next, pqueue_pri_t curr)
{
    return (next < curr);
}


static pqueue_pri_t
get_pri(void *a)
{
    return ((task_t *) a)->pri;
}


static void
set_pri(void *a, pqueue_pri_t pri)
{
    ((task_t *) a)->pri = pri;
}


static size_t
get_pos(void *a)
{
    return ((task_t *) a)->pos;
}


static void
set_pos(void *a, size_t pos)
{
    ((task_t *) a)->pos = pos;
}

pqueue_t* task_queue_init() {
  return pqueue_init(100, cmp_pri, get_pri, set_pri, get_pos, set_pos);
}