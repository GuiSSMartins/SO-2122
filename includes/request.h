#ifndef __REQUEST__
#define __REQUEST__

typedef struct request {
    int n_transfs; // nº de transformações
    char argv[64][64]; // conjunto dos argumentos recebidos
    int pid; // Pid
} Request;

#endif