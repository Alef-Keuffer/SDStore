#include <sys/unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"

const char *const SERVER = "SERVER";

transformation_t transformation_str_to_enum__ (const char *transformation_string)
{
  if (!strcmp ("bcompress", transformation_string))
    return BCOMPRESS;
  else if (!strcmp ("bdecompress", transformation_string))
    return BDECOMPRESS;
  else if (!strcmp ("decrypt", transformation_string))
    return DECRYPT;
  else if (!strcmp ("encrypt", transformation_string))
    return ENCRYPT;
  else if (!strcmp ("gcompress", transformation_string))
    return GCOMPRESS;
  else if (!strcmp ("gdecompress", transformation_string))
    return GDECOMPRESS;
  else if (!strcmp ("nop", transformation_string))
    return NOP;
  else if (!strcmp ("status", transformation_string))
    return STATUS;
  else if (!strcmp ("proc-file", transformation_string))
    return PROC_FILE;
  else
    return -1;
}

transformation_t transformation_str_to_enum (const char *transformation_string)
{
  transformation_t transformation_enum = transformation_str_to_enum__ (transformation_string);
  if (transformation_enum == -1) {
      fprintf (stderr, "transformation_get_value: Unknown transformation %s", transformation_string);
      _exit (EXIT_FAILURE);
  }
  return transformation_enum;
}

const char *transformation_enum_to_str (const transformation_t t)
{
  switch (t)
    {
      case BCOMPRESS:
        return "bcompress";
      case BDECOMPRESS:
        return "bdecompress";
      case DECRYPT:
        return "decrypt";
      case ENCRYPT:
        return "encrypt";
      case GCOMPRESS:
        return "gcompress";
      case GDECOMPRESS:
        return "gdecompress";
      case NOP:
        return "nop";
      default:
        {
          fprintf (stderr, "transformation_enum_to_str: not valid transformation");
          _exit (EXIT_FAILURE);
        }
    }
}
