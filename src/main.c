#include <unistd.h>
#include <sys/stat.h> /*for mkfifo*/
#include <fcntl.h>
#include <stdio.h> /*for perror*/
#include <stdlib.h> /*for exit*/
#include <errno.h>

#include "util.h"
#include "env.h"

/*!

@mainpage Example use

@startuml{myimage.svg} "Example use" width=5cm

skinparam Shadowing false
skinparam defaultFontName monospace

actor User
participant Client
participant Server
actor Admin

activate User

== Initialization ==
activate Admin
Admin -> Server: ./sdstored configFilePath transformationsFolderPath
deactivate Admin
activate Server

== proc-file ==
User -> Client: ./sdstore proc-file priority inPath outPath [transformations]
activate Client
Client -> Server: ?
Server -> Client: ?
Client -> User: pending
Server -> Client: ?
Client -> User: processing
Server -> Client: ?
Client -> User: finished + bytes read + bytes written
deactivate Client

== status ==
User -> Client: ./sdstore status
activate Client
Client -> Server: ?
Server -> Client: ?
Client -> User: tasks + transformations
deactivate Client

== Termination ==

Server <-? : SIGTERM
deactivate Server


@enduml
 *
*/

/*! @addtogroup transformations
 * @{ */



void transformation_apply (const char *transf, const char *src, const char *dst)
{
#include <string.h>

  char file[50] = "bin/sdstore-transformations/";
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

void load_config (const char *config, ENV env)
{
  fprintf (stderr, "loading config...");
  const unsigned int BUF_SIZE = 8192;
  char buf[BUF_SIZE];
  int fd = oopen (config, O_RDONLY);
  ssize_t nread = rread (fd, buf, BUF_SIZE);
  cclose (fd);

  for (int i = 0; i < nread; i++)
    {

    }

  fprintf (stderr, "finished loading config");
}

int main ()
{
  return 0;
}
