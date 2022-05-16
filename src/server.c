#include <sys/fcntl.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "util/common.h"
#include "util/pqueue.h"
#include "util/pqueue_task.h"
#include "util/safe.h"
#include "_server.h"

enum {
  GLOBAL_MAX_PARALLEL_TASKS = 256
};
struct {
  volatile sig_atomic_t has_been_interrupted;
  const int max_parallel_task;
  task_t *active_tasks[GLOBAL_MAX_PARALLEL_TASKS];
  const char *TRANSFORMATIONS_FOLDER;
  int server_fifo_rd;
  int server_fifo_wr;
  pqueue_t *queue;
  int num_active_tasks;
  int get_transformation_active_limit[NUMBER_OF_TRANSFORMATIONS];
  int get_transformation_active_count[NUMBER_OF_TRANSFORMATIONS];
} g = {
    .has_been_interrupted = 0,
    .num_active_tasks = 0,
    .get_transformation_active_count = {0},
    .active_tasks = {NULL}
};

void exec (transformation_t transformation)
{
  char command[BUFSIZ];
  const char *transformation_str = transformation_enum_to_str (transformation);
  strncpy (command, g.TRANSFORMATIONS_FOLDER, BUFSIZ);
  strncat (command, transformation_str, BUFSIZ);
  fprintf (stderr, "[%ld] execl(%s)\n", (long) getpid (), command);
  eexecl (command, transformation_str, (char *) NULL);
}

void pipe_progs (const task_t *task)
{
  const char *src = task->src;
  const char *dst = task->dst;
  const char *ops = task->ops;
  const int program_num = task->num_ops;

  int fd;
  int pips[program_num - 1][2];

  const int last = program_num - 1;

  static const char READ_END = 0;
  static const char WRITE_END = 1;

  for (int cs = 0; cs < program_num; ++cs)
    {
      if (cs != last)
        ppipe (pips[cs]);

      if (ffork ())
        {
          if (cs != last)
            cclose (pips[cs][WRITE_END]);
          if (cs != 0)
            cclose (pips[cs - 1][READ_END]);
        }
      else
        {
          if (cs != last)
            cclose (pips[cs][READ_END]);
          if (cs == 0)
            {
              fd = oopen (src, O_RDONLY);
              ddup2 (fd, STDIN_FILENO);
              cclose (fd);
            }
          if (cs == last)
            {
              fd = oopen (dst, O_WRONLY);
              ddup2 (fd, STDOUT_FILENO);
              cclose (fd);
            }
          if (cs != last)
            {
              ddup2 (pips[cs][WRITE_END], STDOUT_FILENO);
              cclose (pips[cs][WRITE_END]);
            }
          if (cs != 0)
            {
              ddup2 (pips[cs - 1][READ_END], STDIN_FILENO);
              cclose (pips[cs - 1][READ_END]);
            }
          exec (ops[cs]);
        }
    }

  pid_t pid;
  int status;
  while ((pid = waitpid (-1, &status, 0)) > 0)
    fprintf (stderr, "[%ld] pipe_progs: %ld has finished; WIFEXITED: %d; WEXITSTATUS: %d\n", (long) getpid (), (long) pid, WIFEXITED(status), WEXITSTATUS(status));
  fprintf (stderr, "[%ld] pipe_progs: finished\n", (long) getpid ());
}

int task_is_possible (const task_t *task)
{
  for (int j = 0; j < NUMBER_OF_TRANSFORMATIONS; ++j)
    if (g.get_transformation_active_count[j] + task->ops_totals[j] > g.get_transformation_active_limit[j])
      return 0;

  return 1;
}

void increment_active_transformations_count (const task_t *task)
{
  for (int j = 0; j < NUMBER_OF_TRANSFORMATIONS; ++j)
    g.get_transformation_active_count[j] += task->ops_totals[j];
}

/*!
 * @param task an innactive task
 * @return an active task (a monitor is executing it)
 */
task_t *monitor_run_task (task_t *task)
{
  fprintf (stderr, "[%ld] dispatching task for %s\n", (long) getpid (), task->client_pid_str);

  increment_active_transformations_count (task);

  {
    int monitor_pid = ffork ();
    if (monitor_pid)
      {
        task->monitor = monitor_pid;
        return task;
      }
  }

  /* Without these ignore when, for example, ctrl-c is pressed in terminal
   * all child exec processes are terminated. */
  signal (SIGINT, SIG_IGN);
  signal (SIGTERM, SIG_IGN);

  int client_fd = oopen (task->client_pid_str, O_WRONLY);
  wwrite (client_fd, "processing\n", 11);

  pipe_progs (task);

  /*
   * Semantics is important. Notice the purpose of the two functions:
   *
   * 1. `open`: Open a file for reading or writing
   * 2. `stat`: Get file status, returns information about a file
   *
   * Furthermore,
   *
   * """
   * A call to `open()` creates a new open file description, an entry in
   * the system-wide table of open files.
   * """ (https://man7.org/linux/man-pages/man2/open.2.html)
   *
   * Using lseek(fd,0,SEEK_END) to obtain the file size is a trick that can be avoided
   * with the more appropriate function `stat`.
   */

  struct stat st;

  stat (task->src, &st);
  const off_t bytes_input = st.st_size;

  stat (task->dst, &st);
  const off_t bytes_output = st.st_size;

  char completed_message[200];
  int completed_message_length = snprintf (completed_message, 200, "completed (bytes-input: %ld, bytes-output: %ld)\n\x80", bytes_input, bytes_output);

  wwrite (client_fd, completed_message, completed_message_length);
  cclose (client_fd);

  char buf[128];
  const int nbytes = snprintf (buf, 128, "%c%ld", FINISHED_TASK, (long) getpid ());
  wwrite (g.server_fifo_wr, buf, nbytes + 1);
  fprintf (stderr, "[%ld] monitor exiting\n", (long) getpid ());

  _exit (EXIT_SUCCESS);
}

void free_task (task_t *task)
{
  free (task->client_pid_str);
  free (task->ops);
  free (task->src);
  free (task->dst);
  free (task);
}

/*!
 * @param[out] stat_message `\x80` terminated string with server status
 * @return length of string
 */
size_t get_status_str (char stat_message[BUFSIZ])
{
  stat_message[0] = 0;
  char buf[BUFSIZ];
  snprintf (buf, BUFSIZ, "Number of active tasks: %d\n", g.num_active_tasks);
  strcat (stat_message, buf);

  for (int i = 0; i < GLOBAL_MAX_PARALLEL_TASKS; ++i)
    {
      const task_t *task = g.active_tasks[i];
      if (task == NULL)
        continue;
      snprintf (buf, BUFSIZ, "task [pid=%s]: proc-file %llu %s %s", task->client_pid_str, task->pri, task->src, task->dst);
      strcat (stat_message, buf);
      for (int j = 0; j < task->num_ops; ++j)
        {
          strcat (stat_message, " ");
          strcat (stat_message, transformation_enum_to_str (task->ops[j]));
        }
      strcat (stat_message, "\n");
    }
  for (int i = 0; i < NUMBER_OF_TRANSFORMATIONS; ++i)
    {
      snprintf (buf, BUFSIZ, "transf %s: %d/%d (running/max)\n", transformation_enum_to_str (i), g.get_transformation_active_count[i], g.get_transformation_active_limit[i]);
      strcat (stat_message, buf);
    }
  strcat (stat_message, "\x80");
  return strlen (stat_message);
}

void decrement_active_transformations_count (const task_t *task)
{
  for (int j = 0; j < NUMBER_OF_TRANSFORMATIONS; ++j)
    g.get_transformation_active_count[j] -= task->ops_totals[j];
}

/*!
 * task_message ::= ⟨proc_file⟩ | ⟨status⟩ | ⟨finished_task⟩
 *      ⟨proc_file⟩ ::= ⟨client_pid_str⟩ ⟨PROC_FILE⟩ ⟨priority⟩ ⟨src⟩ ⟨dst⟩ ⟨num_ops⟩ ⟨ops⟩⁺
 *      ⟨status⟩    ::= ⟨client_pid_str⟩ ⟨STATUS⟩
 *      ⟨finished_taks⟩ ::= ⟨FINISHED_TASK⟩ ⟨monitor_pid_str⟩
 *
 * ⟨num_ops⟩ ::= ⟨int⟩
 *
 * @param m ≡ task_message
 */
void process_message (char *m)
{
  fprintf (stderr, "[%ld] process_message: %s\n", (long) getpid (), m);

  if (m[0] == FINISHED_TASK)
    {
      fprintf (stderr, "[%ld] process_message: finished_task\n", (long) getpid ());
      pid_t monitor_pid = sstrtol (&m[1]);
      // search for task that was being executed by the monitor that just finished
      int finished_task_index;
      for (finished_task_index = 0; finished_task_index < GLOBAL_MAX_PARALLEL_TASKS; ++finished_task_index)
        if (g.active_tasks[finished_task_index] != NULL
            && g.active_tasks[finished_task_index]->monitor == monitor_pid)
          break;
      assert (finished_task_index < GLOBAL_MAX_PARALLEL_TASKS);
      decrement_active_transformations_count (g.active_tasks[finished_task_index]);
      free_task (g.active_tasks[finished_task_index]);
      g.active_tasks[finished_task_index] = NULL;
      --g.num_active_tasks;
      return;
    }

  const char del[2] = "\31";
  char *tok;

  tok = strtok (m, del);
  const char *client_pid_str = tok;
  fprintf (stderr, "[%ld] client_pid_str = %s\n", (long) getpid (), client_pid_str);

  tok = strtok (NULL, del);
  const char *command = tok;
  fprintf (stderr, "[%ld] command = %s\n", (long) getpid (), command);

  if (transformation_str_to_enum (command) == STATUS)
    {
      char status[BUFSIZ];
      const size_t n = get_status_str (status);
      const int fd = oopen (client_pid_str, O_WRONLY);
      wwrite (fd, status, n);
      cclose (fd);
      fprintf (stderr, "[%ld] status: for client %s\n", (long) getpid (), client_pid_str);
      return;
    }

  task_t *task = mmalloc (sizeof (task_t));

  task->client_pid_str = mmalloc (strlen (client_pid_str) + 1);
  strcpy (task->client_pid_str, client_pid_str);
  fprintf (stderr, "[%ld] task->client_pid_str = %s\n", (long) getpid (), task->client_pid_str);

  tok = strtok (NULL, del);
  task->pri = sstrtol (tok);
  fprintf (stderr, "[%ld] task->pri = %llu\n", (long) getpid (), task->pri);

  tok = strtok (NULL, del);
  task->src = mmalloc (strlen (tok) + 1);
  strcpy (task->src, tok);
  fprintf (stderr, "[%ld] task->src = %s\n", (long) getpid (), task->src);

  tok = strtok (NULL, del);
  task->dst = mmalloc (strlen (tok) + 1);
  strcpy (task->dst, tok);
  fprintf (stderr, "[%ld] task->dst = %s\n", (long) getpid (), task->dst);

  tok = strtok (NULL, del);
  task->num_ops = sstrtol (tok);
  task->ops = mmalloc (task->num_ops);
  fprintf (stderr, "[%ld] task->num_ops = %d\n", (long) getpid (), task->num_ops);

  memset (task->ops_totals, 0, NUMBER_OF_TRANSFORMATIONS);
  tok = strtok (NULL, del);
  for (int i = 0; tok != NULL; ++i, tok = strtok (NULL, del))
    {
      transformation_t transformation = transformation_str_to_enum (tok);
      task->ops[i] = transformation;
      ++task->ops_totals[transformation];
    }

  for (transformation_t i = 0; i < NUMBER_OF_TRANSFORMATIONS; ++i)
    fprintf (stderr, "[%ld] task->ops_totals[%s] = %d\n", (long) getpid (), transformation_enum_to_str (i), task->ops_totals[i]);

  for (transformation_t i = 0; i < NUMBER_OF_TRANSFORMATIONS; ++i)
    {
      if (task->ops_totals[i] > g.get_transformation_active_limit[i])
        {
          int fd = oopen (task->client_pid_str, O_WRONLY);
          char task_is_not_possible[BUFSIZ];
          const char *transformation_name = transformation_enum_to_str (i);
          int n = snprintf (task_is_not_possible, BUFSIZ, "Task has more operations (%s = %d > limit[%s] = %d) than current server supports. Therefore this task is impossible and will not be processed.\n\x80", transformation_name, task->ops_totals[i], transformation_name, g.get_transformation_active_limit[i]);
          wwrite (fd, task_is_not_possible, n);
          cclose (fd);
          return;
        }
    }

  fprintf (stderr, "[%ld] task->ops =", (long) getpid ());
  for (int i = 0; i < task->num_ops; ++i)
    fprintf (stderr, " %s", transformation_enum_to_str (task->ops[i]));
  fprintf (stderr, "\n");

  pqueue_insert (g.queue, task);

  int fd = oopen (task->client_pid_str, O_WRONLY);
  wwrite (fd, "pending\n", 8);
  cclose (fd);
}

void sig_handler (__attribute__((unused)) int signum)
{
  unlink (SERVER);
  g.has_been_interrupted = 1;
}

void block_read_fifo ()
{
  char buf[BUFSIZ];
  // assume client requests will not fill pipe buffer

  fprintf (stderr, "[%ld] will block on fifo read\n", (long) getpid ());
  size_t nbytes = rread (g.server_fifo_rd, buf, BUFSIZ);
  fprintf (stderr, "[%ld] unblocked from fifo read\n", (long) getpid ());
  if (!g.has_been_interrupted)
    assert(nbytes > 0);
  assert(nbytes < BUFSIZ);
  fprintf (stderr, "[%ld] block_read: read(s=%zu) %.*s\n", (long) getpid (), nbytes, (unsigned int) nbytes, buf);
  for (size_t i = 0; i < nbytes;)
    {
      size_t j = strlen (&buf[i]) + 1;
      process_message (&buf[i]);
      i += j;
    }
}

/*!
 * @return the smallest index at @p active_tasks that is not being used.
 */
int next_pos (const int max_parallel_tasks, task_t *active_tasks[max_parallel_tasks])
{
  for (int i = 0; i < max_parallel_tasks; ++i)
    if (active_tasks[i] == NULL)
      return i;

  fprintf (stderr, "ERROR: reached maximum number of parallel tasks\n");
  _exit (EXIT_FAILURE);
}

static inline int queue_is_empty ()
{ return pqueue_size (g.queue) == 0; }

void listening_loop ()
{
  fprintf (stderr, "[%ld] entered listening_loop\n", (long) getpid ());

  while (1)
    {
      fprintf (stderr, "[%ld] looping in listening_loop\n", (long) getpid ());
      block_read_fifo ();

      if (g.has_been_interrupted && queue_is_empty () && g.num_active_tasks == 0)
        break;

      for (task_t *task = pqueue_peek (g.queue); task != NULL; task = pqueue_peek (g.queue))
        {
          if (task_is_possible (task))
            {
              const int i = next_pos (GLOBAL_MAX_PARALLEL_TASKS, g.active_tasks);
              task_t *active_task = monitor_run_task (pqueue_pop (g.queue));
              g.active_tasks[i] = active_task;
              ++g.num_active_tasks;
            }
          else
            break;
        }
    }
}

void sig_child (__attribute__((unused)) int signum)
{ wwaitpid (-1, NULL, 0); }

int main (__attribute__((unused)) int argc, char *argv[])
{
  struct sigaction sa;
  sigemptyset (&sa.sa_mask);
  sa.sa_handler = sig_handler;
  sa.sa_flags = 0;

  sigaction (SIGINT, &sa, NULL);
  sigaction (SIGTERM, &sa, NULL);

  /*
   * POSIX.1-1990 disallowed setting the action for SIGCHLD to
   * SIG_IGN.  POSIX.1-2001 and later allow this possibility, so that
   * ignoring SIGCHLD can be used to prevent the creation of zombies
   * (see wait(2)).  Nevertheless, the historical BSD and System V
   * behaviors for ignoring SIGCHLD differ, so that the only
   * completely portable method of ensuring that terminated children
   * do not become zombies is to catch the SIGCHLD signal and perform
   * a wait(2) or similar. (https://man7.org/linux/man-pages/man2/sigaction.2.html)
   */
  signal (SIGCHLD, sig_child);

  fprintf (stderr, "[%ld] loading config...\n\n", (long) getpid ());
  char buf[BUFSIZ];
  const char *config_file = argv[1];
  int fd = oopen (config_file, O_RDONLY);
  rread (fd, buf, BUFSIZ);
  cclose (fd);
  fprintf (stderr, "[%ld] config payload:\n\n```\n%s\n```\n\n", (long) getpid (), buf);

  // if a file with the same name as our fifo exists we delete it
  if (access (SERVER, F_OK) == 0)
    {
      fprintf (stderr, "[%ld] deleting file %s which uses same name as our private fifo.", (long) getpid (), SERVER);
      unlink (SERVER);
    }

  g.queue = task_queue_init ();

  /* loading config: parses and loads the configuration file.
   * notices that we assume a very specific format,
   * in particular, we do not deal with arbitrary whitespaces.
   *
   * This can be later put on its own function.
   * */
  int i = 0;
  while (buf[i] != 0)
    {
      int j;
      for (j = i; buf[i] != ' '; ++i); // !isspace()
      buf[i++] = 0;
      transformation_t transformation = transformation_str_to_enum (&buf[j]);
      for (j = i; buf[i] != 0 && buf[i] != '\n'; ++i);
      buf[i++] = 0;
      g.get_transformation_active_limit[transformation] = sstrtol (&buf[j]);
    }

  // print the limits we loaded to the screen (just more debug info)
  fprintf (stderr, "[%ld] loaded config. Config is: \n\n```\n", (long) getpid ());
  for (transformation_t j = 0; j < NUMBER_OF_TRANSFORMATIONS; ++j)
    fprintf (stderr, "limits[%s] = %d\n", transformation_enum_to_str (j), g.get_transformation_active_limit[j]);
  fprintf (stderr, "```\n\n");
  g.TRANSFORMATIONS_FOLDER = argv[2];
  fprintf (stderr, "[%ld] g.TRANSFORMATIONS_FOLDER = %s\n\n", (long) getpid (), g.TRANSFORMATIONS_FOLDER);

  mmkfifo (SERVER, S_IRWXU);
  fprintf (stderr, "[%ld] Created fifo %s\n", (long) getpid (), SERVER);

  g.server_fifo_rd = oopen (SERVER, O_RDONLY);
  fprintf (stderr, "[%ld] Opened fifo %s for read\n", (long) getpid (), SERVER);
  if (g.has_been_interrupted)
      _exit (EXIT_SUCCESS);

  /* Opening for write only happens after first client writes to pipe
   * since the command above blocks until someone opens pipe for writing */
  g.server_fifo_wr = oopen (SERVER, O_WRONLY);
  fprintf (stderr, "[%ld] Opened fifo %s for write\n", (long) getpid (), SERVER);

  listening_loop ();
  fprintf(stderr,"[%ld] Exited listening_loop\n", (long) getpid ());

  cclose (g.server_fifo_rd);
  fprintf(stderr,"[%ld] Closed g.server_fifo_rd\n", (long) getpid ());
  cclose (g.server_fifo_wr);
  fprintf(stderr,"[%ld] Closed g.server_fifo_wr\n", (long) getpid ());

  unlink (SERVER);
  fprintf (stderr, "[%ld] Unlinked server fifo\n", (long) getpid ());
  return 0;
}

