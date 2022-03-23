#include <ctype.h> /*isspace*/
#include <string.h> /*strcmp*/
#include "command.h"

enum COMMAND parse_command (const char *line, unsigned int *i)
{
  const unsigned int BUF_SIZE = 100;
  char buf[BUF_SIZE];
  unsigned int j;
  while (isspace (line[*i])) *i += 1;
  for (j = 0; *i < BUF_SIZE && !isspace(line[*i]); j++, *i += 1)
    buf[j] = line[*i];
  buf[j] = '\0';

  if (!strcmp ("proc-file", buf))
    return proc_file;
  else if (!strcmp ("status", buf))
    return status;
  else
    return -1;
}