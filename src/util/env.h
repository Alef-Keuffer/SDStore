#ifndef SDSTORE_ENV_H
#define SDSTORE_ENV_H
#include <unistd.h>

typedef struct env {
  unsigned int limits[7];
  unsigned int running[7];
  char *transformations_path;
  ssize_t server;
  ssize_t client;
} *ENV;
#endif //SDSTORE_ENV_H
