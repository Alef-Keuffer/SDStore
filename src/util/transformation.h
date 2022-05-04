#ifndef _TRANSFORMATION_H_
#define _TRANSFORMATION_H_
#include "env.h"

const int nTransformations = 7;
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

TRANSFORMATION parse_transformation (const char *string, unsigned int *i);
void print_transformations (const TRANSFORMATION transformations[]);
void transformation_apply (const char *transf, const char *src, const char *dst, const char *transformation_path);
void apply_transformations(const TRANSFORMATION transformations[], const char *src, const char *dst, const char *transformation_path);
int transformation_is_possible(ENV e, const int totals[nTransformations]);
void transformation_get_totals(TRANSFORMATION t[], int totals[nTransformations]);
void transformation_sub_from_running(ENV e, const int totals[nTransformations]);
void transformation_add_to_running(ENV e, const int totals[nTransformations]);
#endif //_TRANSFORMATION_H_
