#ifndef __REPLY__
#define __REPLY__

typedef struct reply {
    int argc;
    char argv[64][516];
    int flag; // 0 -> fechar a fifo
              // 1 -> para continuar o programa
} Reply;

#endif