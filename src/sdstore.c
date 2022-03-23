#include <unistd.h>
#include <fcntl.h>
#include "util.h"
#include "sdstored.h"
const char *NPIPE_TO_SERVER = "toServer";
int main(int argc, char *argv[]) {
  int fd = oopen(NPIPE_TO_SERVER,O_WRONLY);
  const unsigned int comLen = 39;
  char com[39] = "192 proc-file 2 filea fileacomp nop nop";
  write(fd,com,comLen);
  close(fd);
}
