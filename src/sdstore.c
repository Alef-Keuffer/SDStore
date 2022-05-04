#include <unistd.h>
#include <fcntl.h>
#include<signal.h>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "util.h"

void sig_handler(int signum){
  //Return type of the handler function should be void
  printf("\nInside handler function\n");
}

const int NPIPE_PERMISSION = 0666;
const char *NPIPE_TO_SERVER = "toServer";

int main(int argc, char *argv[]) {

  // make request
  char comm[8192] = "";
  char NPIPE_TO_CLIENT[32] = {0};
  snprintf (NPIPE_TO_CLIENT, 32, "%ld", (long)getpid());
  mkfifo(NPIPE_TO_CLIENT,NPIPE_PERMISSION);

  strcat(comm,NPIPE_TO_CLIENT);
  for (int i = 1; i < argc; i++) {
    strcat(comm," ");
    strcat(comm,argv[i]);
  }
  fprintf(stderr,"comm = %s\n",comm);
  int fd_write = oopen(NPIPE_TO_SERVER, O_WRONLY);
  write(fd_write, comm, strlen(comm));

  ssize_t ret;
  char buf[8192];
  //while((ret = read(fd_read,buf,8192)) > 0) write(STDOUT_FILENO,buf,ret);

  //close(fd_write);
  //int fd_read = open(NPIPE_TO_CLIENT,O_RDONLY);
  //close(fd_read);
  //unlink(NPIPE_TO_CLIENT); // delete client receiver
}
