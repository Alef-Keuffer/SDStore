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

static const int READ_END = 0;
static const int WRITE_END = 1;

/*

S - Size (0-255)

S ⟨PID⟩ S ⟨src⟩ S ⟨dst⟩ S ⟨op⟩⁺

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
}

int pipe_progs_original (const char *m)
{
  /*m ≡ S ⟨src⟩ S ⟨dst⟩ S ⟨op⟩⁺*/
  const char *src = m + 1;
  fprintf (stderr, "pipe_progs: src is %s\n", src);
  const unsigned char s_src = src[-1];
  fprintf (stderr, "pipe_progs: s_src is %d\n", s_src);
  const char *dst = src + s_src + 1;
  fprintf (stderr, "pipe_progs: dst is %s\n", dst);
  const unsigned char s_dst = dst[-1];
  fprintf (stderr, "pipe_progs: s_dst is %d\n", s_dst);
  const char *ops = dst + s_dst + 1;
  const int n = (unsigned char) ops[-1];
  fprintf (stderr, "pipe_progs: number of ops is %d\n", n);

  int fd;
  int fds[2 * (n - 1)];


  if (n == 1)
    {
      if (fork () != 0)
        return 0;

      fprintf (stderr, "[%ld]: %s → p_0 → %s\n", (long) getpid (), src, dst);

      fd = open (src, O_RDONLY);
      dup2 (fd, STDIN_FILENO);
      close (fd);

      fd = open (dst, O_WRONLY);
      dup2 (fd, STDOUT_FILENO);
      close (fd);
      exec (ops[0]);
    }

  for (int i = 0; i < n; i++)
    {
      if (i < n - 1)
        {
          pipe (fds + 2 * i);
          fprintf (stderr, "[%ld]: (fds[%d],fds[%d])\n", (long) getpid (), 2 * i, 2 * i + 1);
        }
      if (fork () != 0) {
        int c = i%2 ? 2*(i-1) : 2*i+1; //then close previous else close next
        close(fds[c]);
        fprintf (stderr, "[%ld]: close(fds[%d])\n", (long) getpid (),c);
        continue;
      }
      if (i == 0)
        {
          close (fds[2 * i]);
          fd = open (src, O_RDONLY);
          dup2 (fd, 0);
          close (fd);
          dup2 (fds[2 * i + 1], 1);
          fprintf (stderr, "[%ld]: %s → p_0 → fds[%d]\n", (long) getpid (), src, 2 * i + 1);
        }
      else if (i == n - 1)
        {
          close (fds[2 * i - 1]);
          fd = open (dst, O_WRONLY);
          dup2 (fd, 1);
          close (fd);
          dup2 (fds[2 * (i - 1)], 0);
          fprintf (stderr, "[%ld]: fds[%d] → p_%d → %s\n", (long) getpid (), 2 * (i - 1), i, dst);
        }
      else
        {
          dup2 (fds[2 * (i - 1)], 0);
          dup2 (fds[2 * i + 1], 1);
          fprintf (stderr, "[%ld]: fds[%d] → p_%d → fds[%d]\n", (long) getpid (), 2 * (i - 1), i, 2 * i + 1);
        }
      exec (ops[i]);
    }
  return 0;
}

pid_t pipe_progs2 (const task_t * task)
{
  fprintf (stderr, "[%ld] pipe_progs\n", (long) getpid ());
  int monitor_pid;
  if ((monitor_pid = fork ()))
    return monitor_pid;

  const char *src = task->src;
  const char *dst = task->dst;
  const char *ops = task->ops;
  const int n = task->num_ops;

  int fd;
  int fds[n - 1][2];

  if (n == 1)
    {
      if (fork () != 0)
        {
          wait (NULL);
          _exit (0);
        }

      fprintf (stderr, "[%ld]: %s → p_0 → %s\n", (long) getpid (), src, dst);

      fd = open (src, O_RDONLY);
      dup2 (fd, STDIN_FILENO);
      cclose (fd);

      fd = oopen (dst, O_WRONLY);
      dup2 (fd, STDOUT_FILENO);
      cclose (fd);
      exec (ops[0]);
    }

  for (int i = 0; i < n; i++)
    {
      if (i < n - 1)
        {
          pipe (fds[i]);
          //fprintf (stderr, "[%ld]: (fds[%d],fds[%d])\n", (long) getpid (), 2 * i, 2 * i + 1);
        }
      if (fork ())
        {
          int c = i % 2 ? 2 * (i - 1) : 2 * i + 1; //then close previous else close next
          fprintf (stderr, "[%ld]: closing(fds[%d])\n", (long) getpid (), c);
          cclose (fds[c]);
          fprintf (stderr, "[%ld]: closed(fds[%d])\n", (long) getpid (), c);
          continue;
        }
      if (i == 0)
        {
          cclose (fds[0][READ_END]);
          fd = oopen (src, O_RDONLY);
          dup2 (fd, STDIN_FILENO);
          cclose (fd);
          dup2 (fds[0][WRITE_END], STDOUT_FILENO);
          //fprintf (stderr, "[%ld]: %s → p_0 → fds[%d]\n", (long) getpid (), src, 2 * i + 1);
        }
      else if (i == n - 1)
        {
          //fprintf (stderr, "[%ld]: closing(fds[%d])\n", (long) getpid (), 2*i-1);
          cclose (fds[n - 1][WRITE_END]);
          fd = oopen (dst, O_WRONLY);
          dup2 (fd, STDOUT_FILENO);
          cclose (fd);
          dup2 (fds[n - 1][READ_END], STDIN_FILENO);
          //fprintf (stderr, "[%ld]: fds[%d] → p_%d → %s\n", (long) getpid (), 2 * (i - 1), i, dst);
        }
      else
        {
          dup2 (fds[i - 1][READ_END], STDIN_FILENO);
          dup2 (fds[i][WRITE_END], STDOUT_FILENO);
          //fprintf (stderr, "[%ld]: fds[%d] → p_%d → fds[%d]\n", (long) getpid (), 2 * (i - 1), i, 2 * i + 1);
        }
      for (int j = 0; j < n - 1; ++i)
        {
          cclose (fds[j][READ_END]);
          cclose (fds[j][WRITE_END]);
        }
      exec (ops[i]);
    }

  while (waitpid (-1, NULL, 0) > 0)
    {
      fprintf (stderr, "pipe_progs: waited\n");
    }
  fprintf (stderr, "[%ld] pipe_progs: finished\n", (long) getpid ());
  _exit (0);
}

pid_t pipe_progs (const task_t *task)
{
  fprintf (stderr, "[%ld] pipe_progs\n", (long) getpid ());
  int wpid;
  if ((wpid = fork ()) != 0)
    return wpid;
  const char *src = task->src;
  const char *dst = task->dst;
  const char *ops = task->ops;
  const int n = task->num_ops;

  int fd;
  int fds[n - 1][2];

  for (int p = 0; p < n; p++) {
      int res = pipe(fds[p]);
      if (res < 0) {
          perror("server: pipe creation failed");
          return -1;
      }
  }

  if (n == 1) {
      if (fork () != 0)
        {
          wait (NULL);
          _exit (0);
        }

      fprintf (stderr, "[%ld]: %s → p_0 → %s\n", (long) getpid (), src, dst);

      fd = open (src, O_RDONLY);
      dup2 (fd, STDIN_FILENO);
      cclose (fd);

      fd = oopen (dst, O_WRONLY);
      dup2 (fd, STDOUT_FILENO);
      cclose (fd);
      exec (ops[0]);
    }

  for (int i = 0; i < n; i++) {
      /*if (i < n - 1) {
          pipe (fds[i]);
          //fprintf (stderr, "[%ld]: (fds[%d],fds[%d])\n", (long) getpid (), 2 * i, 2 * i + 1);
        }*/
      if (fork ())
        {
          int c = i % 2 ? 2 * (i - 1) : 2 * i + 1; //then close previous else close next
          fprintf (stderr, "[%ld]: closing(fds[%d])\n", (long) getpid (), c);
          //cclose (fds[c]);
          fprintf (stderr, "[%ld]: closed(fds[%d])\n", (long) getpid (), c);
          continue;
        }
      if (i == 0)
        {
          cclose (fds[0][READ_END]);
          fd = oopen (src, O_RDONLY);
          dup2 (fd, STDIN_FILENO);
          cclose (fd);
          dup2 (fds[0][WRITE_END], STDOUT_FILENO);
          //fprintf (stderr, "[%ld]: %s → p_0 → fds[%d]\n", (long) getpid (), src, 2 * i + 1);
        }
      else if (i == n - 1)
        {
          //fprintf (stderr, "[%ld]: closing(fds[%d])\n", (long) getpid (), 2*i-1);
          cclose (fds[n - 1][WRITE_END]);
          fd = oopen (dst, O_WRONLY);
          dup2 (fd, STDOUT_FILENO);
          cclose (fd);
          dup2 (fds[n - 1][READ_END], STDIN_FILENO);
          //fprintf (stderr, "[%ld]: fds[%d] → p_%d → %s\n", (long) getpid (), 2 * (i - 1), i, dst);
        }
      else
        {
          dup2 (fds[i - 1][READ_END], STDIN_FILENO);
          dup2 (fds[i][WRITE_END], STDOUT_FILENO);
          //fprintf (stderr, "[%ld]: fds[%d] → p_%d → fds[%d]\n", (long) getpid (), 2 * (i - 1), i, 2 * i + 1);
        }

      for (int j = 0; j < n - 1; ++i)
        {
          cclose (fds[j][READ_END]);
          cclose (fds[j][WRITE_END]);
        }
      exec (ops[i]);
    }

  while (waitpid (-1, NULL, 0) > 0)
    {
      fprintf (stderr, "pipe_progs: waited\n");
    }
  fprintf (stderr, "[%ld] pipe_progs: finished\n", (long) getpid ());
  _exit (0);
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
  write (fd, "processing\n", 11);
  cclose (fd);

  // update running
  increment_running_count (task);

  task->monitor = pipe_progs (task->msg + 1 + (unsigned char) task->msg[0] + 1 + 1);
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
  // m ≡ S ⟨client_pid⟩ ⟨proc-file⟩ ⟨priority⟩ S ⟨src⟩ S ⟨dst⟩ S ⟨ops⟩⁺ ⟨EOO⟩
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
  const int msg_size = 1 + pid_str_size + 1 + 1 + 1 + s_src + 1 + s_dst + 1 + num_ops + 1;
  task->msg = malloc (msg_size);
  memcpy (task->msg, m, msg_size);

  for (int j = 0; j < task->num_ops; j++)
    task->ops_totals[j] = 0;

  for (int j = 0; j < task->num_ops; j++)
    ++task->ops_totals[task->ops[j]];

  return task;
}

void blockRead (char *m)
{
  fprintf (stderr, "[%ld] blockRead\n", (long) getpid ());
  char c;
  size_t i = 0;
  g.serverFifo = open (SERVER, O_RDONLY);
  while (read (g.serverFifo, &c, 1) >= 0)
    {
      m[i++] = c; //is reading message

      if (c == EOO) //end of a message, there might be more in the pipe
        break;
    }
  close (g.serverFifo);
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

void get_status_str (char stats[BUFSIZ])
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
}

int queue_is_empty ()
{
  if (globalQueue->size <= 1)
    return 1;
  return 0;
}

void listening_loop ()
{
  fprintf (stderr, "[%ld] listening_loop\n", (long) getpid ());
  char m[BUFSIZ];
  const int maxTasks = 100; //node -> {data = {pid_executioner,t0,t1,t2,t3,t4,t5,t6,pid_client}, node_t* next}
  task_t *pid[maxTasks]; //{pid,t0,t1,t2,t3,t4,t5,t6,pid_client}
  for (int i = 0; i < maxTasks; ++i)
    pid[i] = NULL;
  int wpid;

  while (1)
    {
      while (!g.hasBeenInterrupted && queue_is_empty ())
        {
          blockRead (m);
          char *client = m + 1;
          if (m[m[0] + 1] == STATUS)
            {
              char status[BUFSIZ];
              get_status_str (status);
              speakTo (client, status);
            }
          else
            {
              task_t *task = get_task (m);
              pqueue_insert (globalQueue, task);
              int fd = oopen (task->client_pid_str, O_WRONLY);
              write (fd, "pending\n", 8);
              cclose (fd);
            }
        }

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
          wpid = wait (NULL);
          fprintf (stderr, "[%ld] listening_loop: wpid = %d\n", (long) getpid (), wpid);
          int j = 0;
          for (; j < maxTasks && pid[j]->monitor != wpid; j++);
          decrement_running_count (pid[j]);
          const char *client = pid[j]->client_pid_str;

          int fd = oopen (client, O_WRONLY);
          write (fd, "completed\n", 10);
          cclose (fd);
          fd = oopen (client, O_WRONLY);
          char c = '\x80';
          write (fd, &c, 1);
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
  fprintf (stderr, "[%ld] loading config...\n", (long) getpid ());
  char buf[BUFSIZ];
  const char *configFilename = argv[1];
  int fd = oopen (configFilename, O_RDONLY);
  rread (fd, buf, BUFSIZ);
  cclose (fd);
  fprintf (stderr, "[%ld] config payload:\n'''\n%s\n'''\n", (long) getpid (), buf);

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
  fprintf (stderr, "[%ld] loaded config\n", (long) getpid ());

  // print the limits we loaded to the screen (just more debug info)
  for (int j = 0; j < NUMBER_OF_TRANSFORMATIONS; j++)
    fprintf (stderr, "globalLimits[%d] = %d\n", j, globalLimits[j]);
  TRANSFORMATIONS_FOLDER = argv[2];
  fprintf (stderr, "[%ld] TRANSFORMATIONS_FOLDER = %s\n", (long) getpid (), TRANSFORMATIONS_FOLDER);

  // starts actually being a server
  // register action as callback in listenAs

  mkfifo (SERVER, S_IRWXU);
  fprintf (stderr, "[%ld] Created fifo\n", (long) getpid ());

  strcpy (FIFO, SERVER);
  signal (SIGINT, sig_handler);
  //g.serverFifo = open (SERVER, O_RDONLY);

  listening_loop ();

  //close (g.serverFifo);
  fprintf (stderr, "[%ld] Closed server fifo\n", (long) getpid ());
  unlink (SERVER);
  return 0;
}

/*
S ≡ size

char** tasks = malloc(N);
e ∈ tasks
e → S ⟨client_pid⟩ ⟨proc-file⟩ ⟨priority⟩ S ⟨src⟩ S ⟨dst⟩ S ⟨ops⟩⁺ ⟨EOO⟩

priority,arrival_time ← {t ∣ t.priority     = max(priorities(tasks))
                           ∧ t.arrival_date = min({q ∈ tasks ∣ q.priority = t.priority}) }


other message for other parts of program:
S ⟨client_pid⟩ ⟨status⟩ ⟨EOO⟩

⟨finished_task⟩ S ⟨ops⟩⁺
*/