#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "util/common.h"

void action(char *ops) {
    fprintf(stderr,"[client] heard: ");
    
    for (int i = 0; ops[i] != EOO; i++) {
        if (ops[i] == 0) {
            fprintf(stderr,"\n");
            fprintf(stderr,"[client] heard: ");
        }
        else
            fprintf(stderr,"%c",ops[i]);
    }
    fprintf(stderr,"\n");
}

int main(int argc, char *argv[]) {
    fprintf(stderr,"argc=%d\n",argc);
    if (argc < 2) {
        fprintf(stderr,"Not enough arguments");
        return 0;
    }

    char ops[BUFSIZ];
    pid_t clientPid = getpid();
    int i = 0;
    i++;
    ops[i-1] = snprintf(ops + i,31,"%d",clientPid) + 1;
    i += ops[i-1];

    int k = 1;
    ops[i++] = parseOp(argv[k++]);

    switch(ops[i-1]) {
        case PROC_FILE:
            /*sdstore proc-file ⟨priority⟩ S ⟨src⟩ S ⟨dst⟩ S ⟨ops⟩⁺*/
            ops[i++] = atoi(argv[k++]); /*priority*/
            i ++;
            ops[i-1] = snprintf(ops+i,255,"%s",argv[k++]) + 1; /*src*/
            fprintf(stderr,"src is %s\n",ops+i);
            i += ops[i-1];
            i++;
            ops[i-1] = snprintf(ops+i,255,"%s",argv[k++]) + 1; /*dst*/
            fprintf(stderr,"dst is %s\n",ops+i);
            i += ops[i-1];
            ops[i++] = argc - k;
            fprintf(stderr,"number of ops is %d\n",ops[i-1]);
            fprintf(stderr,"k=%d\n",k);
            for (; k < argc; k++, i++)
                ops[i] = parseOp(argv[k]);
            break;
        case STATUS:
            fprintf(stderr,"Parsed STATUS\n");
            break;
    }
    ops[i++] = EOO;

    fprintf(stderr,"Message to send is:");
    for (int j = 0; ops[j] != EOO; j++)
        fprintf(stderr," %d",ops[j]);
    fprintf(stderr,"\n");

    speakTo(SERVER,ops);
    fprintf(stderr,"Spoke to '%s'\n",SERVER);
    listenAs(ops+1,action,0);
    fprintf(stderr,"Listened as %s\n",ops+1);

    while(wait(NULL) > 0);

    return 0;
}