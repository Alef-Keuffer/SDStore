#ifndef SDSTORE_ENV_H
#define SDSTORE_ENV_H
#include <unistd.h>
typedef struct env {
  struct {
    unsigned int bcompress;
    unsigned int bdecompress;
    unsigned int decrypt;
    unsigned int encrypt;
    unsigned int gcompress;
    unsigned int gdecompress;
    unsigned int nop;
  } limits;
  struct {
    unsigned int bcompress;
    unsigned int bdecompress;
    unsigned int decrypt;
    unsigned int encrypt;
    unsigned int gcompress;
    unsigned int gdecompress;
    unsigned int nop;
  } running;
  char *transformations_path;
  ssize_t server;
  ssize_t client;
} *ENV;

/*! addtogroup transformations
 * @{ */
typedef enum transformation {
  bcompress,
  bdecompress,
  decrypt,
  encrypt,
  gcompress,
  gdecompress,
  nop
} TRANSFORMATION;

/*I don't know how to extern anonymous structs,*/
static const struct {
  const char *bcompress;
  const char *bdecompress;
  const char *decrypt;
  const char *encrypt;
  const char *gcompress;
  const char *gdecompress;
  const char *nop;
} TRANSFORMATION_NAMES = {
    .bcompress = "bcompress",
    .bdecompress = "bdecompress",
    .decrypt = "decrypt",
    .encrypt = "encrypt",
    .gcompress = "gcompress",
    .gdecompress = "gdecompress",
    .nop = "nop",
};

const char *transformation_get_name (enum transformation t);
//! @} end of group transformations
#endif //SDSTORE_ENV_H
