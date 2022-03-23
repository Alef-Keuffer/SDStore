#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <string.h>

#include "util.h"
#include "sdstored.h"

static const int NPIPE_PERMISSION = 0666;

int main(int argc, char *argv[]) {
  // create/open fifo for client

  /*int fd_read = fifo_create_and_open (NPIPE_TO_CLIENT, NPIPE_PERMISSION, O_RDONLY);
  close(fd_read);
  unlink(NPIPE_TO_CLIENT); // delete client receiver*/

  // make request
  char comm[8192] = "";
  char NPIPE_TO_CLIENT[16];
  snprintf (NPIPE_TO_CLIENT, 16, "%ld", (long)getpid());
  strcat(comm,NPIPE_TO_CLIENT);
  for (int i = 1; i < argc; i++) {
    strcat(comm," ");
    strcat(comm,argv[i]);
  }
  fprintf(stderr,"comm = %s\n",comm);
  int fd_write = oopen(NPIPE_TO_SERVER, O_WRONLY);
  write(fd_write, comm, strlen(comm));
  close(fd_write);
}
