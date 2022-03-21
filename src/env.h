#ifndef SDSTORE_ENV_H
#define SDSTORE_ENV_H
#include <unistd.h>
typedef struct env {
    struct {
        const unsigned int bcompress;
        const unsigned int bdecompress;
        const unsigned int decrypt;
        const unsigned int encrypt;
        const unsigned int gcompress;
        const unsigned int gdecompress;
        const unsigned int nop;
    } limits;
    struct {
        unsigned int bcompress;
        unsigned int bdecompress;
        unsigned int decrypt;
        unsigned int encrypt;
        unsigned int gcompress;
        unsigned int gdecompress;
        unsigned int nop;
    } in_progress;
    ssize_t server;
    ssize_t client;
} *ENV;

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
#endif //SDSTORE_ENV_H
