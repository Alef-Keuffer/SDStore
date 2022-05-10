#ifndef COMMON_H
#define COMMON_H

extern const char* const SERVER;

typedef char transformation_t;

enum {
    EOO = -128, /*END OF (operations)COMMUNICATION*/
    PROC_FILE,
    UPDATE_RUNNING,
    STATUS,
    NOP=0,
    BCOMPRESS,
    BDECOMPRESS,
    ENCRYPT,
    DECRYPT,
    GCOMPRESS,
    GDECOMPRESS,
    NUMBER_OF_TRANSFORMATIONS
};

transformation_t transformation_str_to_enum (const char* transformation_string);
const char *transformation_enum_to_str (transformation_t t);

void speakTo(const char* outFilename, char *ops);

#endif //COMMON_H