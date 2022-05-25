#ifndef __REPLY__
#define __REPLY__

typedef struct reply {
    int n_messages;
    char message[64][516];
    int status; // 0 -> fechar a fifo
                // 1 -> para continuar o programa
} Reply;

#endif