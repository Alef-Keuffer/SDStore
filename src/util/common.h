#ifndef COMMON_H
#define COMMON_H

extern const char* const SERVER;
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

char parseOp (const char* transformation_string);
void speakTo(const char* outFilename, char *ops);
void listenAs(const char* inFilename, void (*action) (char *), int isPassive);
const char *transformation_get_name (const char t);
char* toMessage(char *string);

#endif //COMMON_H