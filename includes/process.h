#ifndef __PROCESS__
#define __PROCESS__

#include "tprocess.h"

typedef struct process {
    int client_pid; // PID do cliente
    int fork_pid; // PID do processo-filho
    TProcess tp[7];  
    int tp_size; // nº de TIPOS de transformações que o processo terá de realizar
                // ex: tp_size = 1 -> só temos de realizar um TIPO de transf, como apenas NOP, ou apenas BCOMPRESS, ... 
    char transf_names[64][64]; // (var. auxiliar) nomes das várias transformações
                              // os nomes das transformações são guaradados pela ordem do comando original 
    char name_input[512]; // nome do ficheiro de input
    char name_output[512]; // nome do ficheiro de output
    int number_transfs; // nº de transformações que se irão realizar
    bool running; // indica se o processo está ativo (ou não)
    bool ready; // indica se está em fila de espera / queue 
} Process; // Processo

#endif