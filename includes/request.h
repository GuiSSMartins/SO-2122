#ifndef __REQUEST__
#define __REQUEST__

typedef struct request {
    int n_messages; // nº de transformações (proc-file)
    // n_args = 1; -> status, >1 (proc-file)
    char message[64][512]; // conjunto dos argumentos recebidos
    int pid; // Pid do Cliente (não é da pipe!)
} Request;

#endif