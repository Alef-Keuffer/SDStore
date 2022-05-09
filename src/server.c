#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "util/common.h"
#include "util/pqueue.h"
#include "_server.h"
#include "util/taskQueue.h"
#include "util/util.h"
#include <assert.h>
static const int READ_END = 0;
static const int WRITE_END = 1;

/*
task_message :: =
    ⟨size⟩ ⟨client_pid_str⟩ ⟨proc-file⟩ ⟨priority⟩ ⟨size⟩ ⟨src⟩ ⟨size⟩ ⟨dst⟩ ⟨size⟩ ⟨ops⟩⁺ ⟨EOO⟩
  | ⟨size⟩ ⟨client_pid_str⟩ ⟨status⟩ ⟨EOO⟩

⟨size⟩ ::= ⟨int⟩

maybe unused:
priority,arrival_time ← {t ∣ t.priority     = max(priorities(tasks))
                           ∧ t.arrival_date = min({q ∈ tasks ∣ q.priority = t.priority}) }
*/

struct {
  int hasBeenInterrupted;
  int isRunning;
  char *TRANSFORMATIONS_FOLDER;
  int serverFifo;
} g = {
    .hasBeenInterrupted = 0,
    .isRunning = 0
};

int num_running_tasks = 0;
int globalLimits[NUMBER_OF_TRANSFORMATIONS];
int globalRunning[NUMBER_OF_TRANSFORMATIONS] = {0};
pqueue_t *globalQueue;
char *TRANSFORMATIONS_FOLDER;

void exec (const char op)
{
  char command[BUFSIZ - 255];
  const char *op_name = transformation_get_name (op);
  strcpy (command, TRANSFORMATIONS_FOLDER);
  strcat (command, op_name);
  fprintf (stderr, "[%ld]: execl(%s)\n", (long) getpid (), command);
  execl (command, op_name, (char *) NULL);
  perror ("execl");
  _exit (EXIT_FAILURE);
}

pid_t pipe_progs (const task_t *task)
{
  fprintf (stderr, "[%ld] pipe_progs\n", (long) getpid ());
  int monitor_pid;
  if ((monitor_pid = fork ()) != 0)
    return monitor_pid;

  const char *src = task->src;
  const char *dst = task->dst;
  const char *ops = task->ops;
  const int program_num = task->num_ops;

  int fd;
  int pips[program_num - 1][2];

  const int last = program_num - 1;

  // If there is only one transformation
  if (program_num == 1)
    {
      if (ffork () != 0)
        {
          wait (NULL);
          _exit (EXIT_SUCCESS);
        }

      fprintf (stderr, "[%ld]: %s → p_0 → %s\n", (long) getpid (), src, dst);

      fd = oopen (src, O_RDONLY);
      ddup2 (fd, STDIN_FILENO);
      cclose (fd);

      fd = oopen (dst, O_WRONLY);
      ddup2 (fd, STDOUT_FILENO);
      cclose (fd);
      exec (ops[0]);
    }

  for (int cs = 0; cs < program_num; cs++)
    {
      if (cs != last)
        ppipe (pips[cs]);
      if (ffork ())
        if (cs == 0)
          cclose (pips[cs][WRITE_END]);
        else if (cs == last)
          cclose (pips[cs - 1][READ_END]);
        else
          {
            cclose (pips[cs][WRITE_END]);
            cclose (pips[cs - 1][READ_END]);
          }
      else
        {
          if (cs == 0)
            {
              cclose (pips[cs][READ_END]);

              fd = oopen (src, O_RDONLY);
              ddup2 (fd, STDIN_FILENO);
              cclose (fd);

              ddup2 (pips[cs][WRITE_END], STDOUT_FILENO);
              cclose (pips[cs][WRITE_END]);
              exec (ops[cs]);
            }
          else if (cs == last)
            {
              fd = oopen (dst, O_WRONLY);
              ddup2 (fd, STDOUT_FILENO);
              cclose (fd);

              ddup2 (pips[cs - 1][READ_END], STDIN_FILENO);
              cclose (pips[cs - 1][READ_END]);
              exec (ops[cs]);
            }
          else
            {
              cclose (pips[cs][READ_END]);
              ddup2 (pips[cs][WRITE_END], STDOUT_FILENO);
              cclose (pips[cs][WRITE_END]);
              ddup2 (pips[cs - 1][READ_END], STDIN_FILENO);
              cclose (pips[cs - 1][READ_END]);
              exec (ops[cs]);
            }
        }
    }

  while (waitpid (-1, NULL, 0) > 0)
    {
      fprintf (stderr, "[%ld] pipe_progs: waited\n", (long) getpid ());
    }
  fprintf (stderr, "[%ld] pipe_progs: finished\n", (long) getpid ());
  _exit (EXIT_SUCCESS);
}

int is_task_possible (const task_t *task)
{
  char opsTotal[NUMBER_OF_TRANSFORMATIONS] = {0};

  for (int j = 0; j < task->num_ops; j++)
    ++opsTotal[task->ops[j]];

  for (int j = 0; j < NUMBER_OF_TRANSFORMATIONS; j++)
    {
      if (globalRunning[j] + opsTotal[j] > globalLimits[j])
        return 0;
    }
  return 1;
}

void increment_running_count (const task_t *task)
{
  for (int j = 0; j < NUMBER_OF_TRANSFORMATIONS; ++j)
    globalRunning[j] += task->ops_totals[j];
}

pid_t dispatchTask (task_t *task)
{
  const char *CLIENT = task->client_pid_str;
  int fd = oopen (CLIENT, O_WRONLY);
  wwrite (fd, "processing\n", 11);
  cclose (fd);

  // update running
  increment_running_count (task);

  task->monitor = pipe_progs (task);
}

void free_task (task_t *task)
{
  free (task->client_pid_str);
  free (task->ops);
  free (task->msg);
  free (task->src);
  free (task->dst);
  free (task);
}

task_t *get_task (const char *m)
{
  // m ≡ ⟨size⟩ ⟨client_pid_str⟩ ⟨proc-file⟩ ⟨priority⟩ ⟨size⟩ ⟨src⟩ ⟨size⟩ ⟨dst⟩ ⟨size⟩ ⟨ops⟩⁺ ⟨EOO⟩
  task_t *task = malloc (sizeof (task_t));

  const char *pid_str = m + 1;
  fprintf (stderr, "[%ld] get_task: pid_str is %s\n", (long) getpid (), pid_str);
  const unsigned char pid_str_size = pid_str[-1];
  fprintf (stderr, "[%ld] get_task: pid_str_size is %d\n", (long) getpid (), pid_str_size);
  const unsigned char priority = m[pid_str_size + 2];
  fprintf (stderr, "[%ld] get_task: priority is %d\n", (long) getpid (), priority);
  const char *src = m + pid_str_size + 4;
  fprintf (stderr, "[%ld] get_task: src is %s\n", (long) getpid (), src);
  const unsigned char s_src = src[-1];
  fprintf (stderr, "[%ld] get_task: s_src is %d\n", (long) getpid (), s_src);
  const char *dst = src + s_src + 1;
  fprintf (stderr, "[%ld] get_task: dst is %s\n", (long) getpid (), dst);
  const unsigned char s_dst = dst[-1];
  fprintf (stderr, "[%ld] get_task: s_dst is %d\n", (long) getpid (), s_dst);
  const char *ops = dst + s_dst + 1;
  const int num_ops = (unsigned char) ops[-1];
  fprintf (stderr, "[%ld] get_task: number of ops is %d\n", (long) getpid (), num_ops);

  task->client_pid_str = malloc (pid_str_size);
  memcpy (task->client_pid_str, pid_str, pid_str_size);
  task->src = malloc (s_src);
  memcpy (task->src, src, s_src);
  task->dst = malloc (s_dst);
  memcpy (task->dst, dst, s_dst);
  task->ops = malloc (num_ops);
  memcpy (task->ops, ops, num_ops);
  task->num_ops = num_ops;
  task->pri = priority;
  // ⟨size⟩ ⟨client_pid_str⟩ ⟨proc-file⟩ ⟨priority⟩ ⟨size⟩ ⟨src⟩ ⟨size⟩ ⟨dst⟩ ⟨size⟩ ⟨ops⟩⁺ ⟨EOO⟩
  const int msg_size =
      1 // ⟨size⟩
      + pid_str_size // ⟨client_pid_str⟩
      + 1 // ⟨proc-file⟩
      + 1 // ⟨priority⟩
      + 1 // ⟨size⟩
      + s_src // ⟨src⟩
      + 1 // ⟨size⟩
      + s_dst // ⟨dst⟩
      + 1 // ⟨size⟩
      + num_ops // ⟨ops⟩⁺
      + 1; // ⟨EOO⟩
  task->msg = malloc (msg_size);
  memcpy (task->msg, m, msg_size);

  for (int j = 0; j < task->num_ops; j++)
    task->ops_totals[j] = 0;

  for (int j = 0; j < task->num_ops; j++)
    ++task->ops_totals[task->ops[j]];

  return task;
}

size_t get_status_str (char stats[BUFSIZ])
{
  stats[0] = 0;
  char buf[BUFSIZ];
  snprintf (buf, BUFSIZ, "Number of running tasks: %d\n", num_running_tasks);
  strcat (stats, buf);
  for (int i = 0; i < NUMBER_OF_TRANSFORMATIONS; ++i)
    {
      snprintf (buf, BUFSIZ, "%s : %d\n", transformation_get_name (i), globalRunning[i]);
      strcat (stats, buf);
    }
  strcat (stats, "\x80");
  return strlen (stats);
}

void deal_with_message (char *m)
{
  char *client = m + 1;
  if (m[m[0] + 1] == STATUS)
    {
      char status[BUFSIZ];
      const size_t n = get_status_str (status);
      const int fd = oopen (client, O_WRONLY);
      wwrite (fd, status, n);
      cclose (fd);
      fprintf (stderr, "[%ld] status: for client %s\n", (long) getpid (), client);
    }
  else
    {
      task_t *task = get_task (m);
      pqueue_insert (globalQueue, task);
      int fd = oopen (task->client_pid_str, O_WRONLY);
      wwrite (fd, "pending\n", 8);
      cclose (fd);
    }
}

void blockRead ()
{
  char m[BUFSIZ];
  fprintf (stderr, "[%ld] blockRead\n", (long) getpid ());
  size_t nbytes;

  // assume client requests will not fill pipe buffer
  nbytes = rread (g.serverFifo, m, BUFSIZ);
  assert(nbytes < BUFSIZ);
  for (size_t i = 0; i < nbytes; ++i)
    if (i == 0 || m[i - 1] == EOO)
      deal_with_message (m + i);
}
void decrement_running_count (const task_t *task)
{
  for (int j = 0; j < NUMBER_OF_TRANSFORMATIONS; ++j)
    globalRunning[j] -= task->ops_totals[j];
}

int next_pos (const int maxTasks, task_t *pid[maxTasks])
{
  int i;
  for (i = 0; i < maxTasks; ++i)
    if (pid[i] == NULL)
      break;
  return i;
}

static inline int queue_is_empty ()
{
  return pqueue_size(globalQueue) == 0;
}

void listening_loop ()
{
  fprintf (stderr, "[%ld] listening_loop\n", (long) getpid ());

  const int maxTasks = 100; //node -> {data = {pid_executioner,t0,t1,t2,t3,t4,t5,t6,pid_client}, node_t* next}
  task_t *pid[maxTasks]; //{pid,t0,t1,t2,t3,t4,t5,t6,pid_client}
  for (int i = 0; i < maxTasks; ++i)
    pid[i] = NULL;

  while (1)
    {
      while (!g.hasBeenInterrupted && queue_is_empty () && num_running_tasks == 0)
        blockRead ();

      if (g.hasBeenInterrupted && queue_is_empty () && num_running_tasks == 0)
        break;

      for (task_t *task = pqueue_peek (globalQueue); globalQueue->size != 0; task = pqueue_peek (globalQueue))
        {
          if (task != NULL && is_task_possible (task))
            {
              int i = next_pos (maxTasks, pid);
              task_t *run_task = pqueue_pop (globalQueue);
              dispatchTask (run_task);
              pid[i] = run_task;
              ++num_running_tasks;
            }
          else
            break;
        }

      // {runningLimit ∨ queueIsEmpty ⟹ cannot execute new tasks}
      if (num_running_tasks > 0)
        {
          // {runningLimit}
          int monitor_pid = wait (NULL);
          fprintf (stderr, "[%ld] listening_loop: monitor %d is finished\n", (long) getpid (), monitor_pid);
          int j = 0;
          for (; j < maxTasks; ++j)
            if (pid[j] != NULL && pid[j]->monitor == monitor_pid)
              break;
          decrement_running_count (pid[j]);
          const char *client = pid[j]->client_pid_str;

          int fd = oopen (pid[j]->src, O_RDONLY);
          const off_t bytes_input = lseek (fd, 0, SEEK_END);
          cclose (fd);

          fd = oopen (pid[j]->dst, O_RDONLY);
          const off_t bytes_output = lseek (fd, 0, SEEK_END);
          cclose (fd);

          char completed_message[200];
          int completed_message_length = snprintf (completed_message, 200, "completed (bytes-input: %ld, bytes-output: %ld)\n", bytes_input, bytes_output);

          fd = oopen (client, O_WRONLY);
          wwrite (fd, completed_message, completed_message_length);
          cclose (fd);

          // this is used to indicate to the client that it can close its listening channel
          const char c = '\x80';
          fd = oopen (client, O_WRONLY);
          wwrite (fd, &c, 1);
          cclose (fd);

          free_task (pid[j]);
          pid[j] = NULL;
          --num_running_tasks;
        }
    }
}

static char FIFO[32];
static void sig_handler (int signum)
{
  signum++; // just for no error about unused variable
  g.hasBeenInterrupted = 1;
  unlink (FIFO);
  _exit (0);
}

int main (int argc, char *argv[])
{
  (void) argc; /* unused parameter */
  fprintf (stderr, "[%ld] loading config...\n\n", (long) getpid ());
  char buf[BUFSIZ];
  const char *configFilename = argv[1];
  int fd = oopen (configFilename, O_RDONLY);
  rread (fd, buf, BUFSIZ);
  cclose (fd);
  fprintf (stderr, "[%ld] config payload:\n\n```\n%s\n```\n\n", (long) getpid (), buf);

  // if a file with the same name as our fifo exists we delete it
  if (access (SERVER, F_OK) == 0)
    unlink (SERVER);

  globalQueue = task_queue_init ();

  /* loading config: parses and loads the configuration file.
   * notices that we assume a very specific format,
   * in particular, we do not deal with arbitrary whitespaces.
   *
   * This can be later put on its own function.
   * */
  int i = 0;
  while (buf[i] != 0)
    {
      int j = i;
      while (buf[i] != ' ') // !isspace()
        i++;
      buf[i++] = 0;
      int op = parseOp (buf + j);
      j = i;
      while (buf[i] != 0 && buf[i] != '\n')
        i++;
      buf[i++] = 0;
      globalLimits[op] = atoi (buf + j);
    }
  fprintf (stderr, "[%ld] loaded config. Config is: \n\n```\n", (long) getpid ());
  // print the limits we loaded to the screen (just more debug info)
  for (int j = 0; j < NUMBER_OF_TRANSFORMATIONS; j++)
    fprintf (stderr, "globalLimits[%d] = %d\n", j, globalLimits[j]);
  fprintf (stderr, "```\n\n");
  TRANSFORMATIONS_FOLDER = argv[2];
  fprintf (stderr, "[%ld] TRANSFORMATIONS_FOLDER = %s\n", (long) getpid (), TRANSFORMATIONS_FOLDER);

  // starts actually being a server
  // register action as callback in listenAs

  mmkfifo (SERVER, S_IRWXU);
  fprintf (stderr, "[%ld] Created fifo\n", (long) getpid ());

  strcpy (FIFO, SERVER);
  signal (SIGINT, sig_handler);
  signal (SIGTERM, sig_handler);

  g.serverFifo = oopen (SERVER, O_RDONLY);
  // opening for write only happens after first client writes to pipe
  // since the command above blocks until someone opens pipe for writing
  fd = oopen (SERVER, O_WRONLY);
  listening_loop ();
  cclose (g.serverFifo);
  cclose (fd);

  fprintf (stderr, "[%ld] Closed server fifo\n", (long) getpid ());
  unlink (SERVER);
  return 0;
}

