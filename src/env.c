#include <stdio.h>
#include "env.h"

const char *transformation_get_name (enum transformation t)
{
  switch (t)
    {
  case bcompress:return TRANSFORMATION_NAMES.bcompress;
  case bdecompress:return TRANSFORMATION_NAMES.bdecompress;
  case decrypt:return TRANSFORMATION_NAMES.decrypt;
  case encrypt:return TRANSFORMATION_NAMES.encrypt;
  case gcompress:return TRANSFORMATION_NAMES.gcompress;
  case gdecompress:return TRANSFORMATION_NAMES.gdecompress;
  case nop:return TRANSFORMATION_NAMES.nop;
  default:
    fprintf(stderr,"transformation_get_name: not valid transformation");
    return "";
    }
}