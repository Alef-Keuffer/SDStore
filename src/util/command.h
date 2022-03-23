#ifndef _COMMAND_H_
#define _COMMAND_H_
enum COMMAND {
  proc_file,
  status
};
enum COMMAND parse_command (const char *line, unsigned int *i);
#endif //_COMMAND_H_
