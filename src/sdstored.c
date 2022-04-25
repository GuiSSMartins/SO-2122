#include <unistd.h> /* chamadas ao sistema: defs e decls essenciais */
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h> // modos de abertura 

#include <stdio.h>
#include <string.h>
#include <stdlib.h> // sizeof
#include <stdbool.h>

// SERVIDOR

// $ ./sdstored config-filename transformations-folder
// $ ./sdstored etc/sdstored.conf bin/sdstore-transformations

char* transf_folder[1024];

// NAME: nop ; bcompress ; bdecompress ; gcompress ; gdecompress ; encrypt ; decrypt
typedef struct transf {
    char name[16];
    char bin[32];
    int running;
    int max;
} Transf; // Transformação

Transf transfs[7]; // Só temos 7 transformações possíveis
int transf_availables = 0;

typedef struct transfs_process {
    int n; // nº de transformações que o processo tem de realizar para a transformação "name"
    char name[16]; // nome da transformação
} TProcess; // TP


typedef struct process {
    int client_pid; // PID do cliente
    int fork_pid; // PID do processo-filho
    TProcess tp[7];  
    int tp_size; // nº de TIPOS de transformações que o processo terá de realizar
                // ex: tp_size = 1 -> só temos de realizar um TIPO de transf, como apenas NOP, ou apenas BCOMPRESS, ... 
    char transf_names[8][16]; // (var. auxiliar) nomes das várias transformações 
    char name_input[1024]; // nome do ficheiro de input
    char name_output[1024]; // nome do ficheiro de output
    int number_transf; // nº de transformações que se irão realizar
    bool active; // indica se o processo está ativo (ou não)
    bool inqueue; // indica se está em fila de espera / queue 
} Process; // Processo


Process processes[1024]; // processos a serem executados ao mesmo tempo (max 1024 processos)
Process process_queue[16][512]; // processos em fila-de-espera (Podemos guardar no máximo 512 processos em cada linha)
// (16 -> indica nº de transformações o que o programa terá de executar no seu processo)
// ex: 2  ->  vai executar 3 transformações no processo atual
// NOTA: Caso o processo execute mais do que 16 transformações, estes também serão guardadaos na linha 15 
int queue_size[16] = {0}; // Nº total de processos na fila-de-espera
int queue_total_size = 0; // Soma total de todos os valores do array anterior
int process_total_size = 0; // Nº total de processos ATIVOS



//         processes[number_process].active = true;

int get_index_transf (char* transf) {
    for (int i=0; i<7; i++) { // Temos apenas no máximo 7 tipos de transformação
        if(strcmp(transf, transfs[i].name) == 0) { // Sáo iguais
            return i;
        }
    }

    return -1; // não encontramos
}

// Recebe apenas processos com apenas 1 transformação (Justificação do [0])
void exec_transf(int number_process) {
    int child_pid = fork();
    int status;
    int fd_input, fd_output;
    if (child_pid == 0) { // Filho -> Transformação

        int transf_index = get_index_transf(processes[number_process].transf_names[0]); // Só uma transformação
        transfs[transf_index].running++;

        char transf_path[128];

        // INPUT
        fd_input = open(processes[number_process].name_input, O_RDONLY);
        dup2(fd_input, STDIN_FILENO); // 0 -> stdin
        close(fd_input);
        
        // OUTPUT
        fd_output = open(processes[number_process].name_output, O_CREAT | O_WRONLY, 0666);
        dup2(fd_output, STDOUT_FILENO); // 1 -> stdout
        close(fd_output);

        strcpy(transf_path, transf_folder);
        strcat(transf_path, transfs[transf_index].bin);
        execl(transf_path, transf_path, NULL);

        _exit(0);
    }
    else { // Pai
        processes[number_process].fork_pid = child_pid;
        wait(&status);
    }
}



ssize_t readln(int fd, char* line, size_t size) {
    int i = 0;
    while (i < size && read(fd, line + i, 1) > 0 && line[i++] != '\n');
    return i;
}

void read_config_file(char* path) {
    char buffer[32];
    int config_file = open(path, O_RDONLY, 0666);
    if (config_file < 0) {
        perror("sdstored: couldn't open file");
    }
    for(int i=0; readln(config_file, buffer, 32) > 0; i++) {
        char* token = strtok(buffer, " ");
        strcpy(transfs[i].name, token);

        char bin[64];
        sprintf(bin, "/bin/%s", transfs[i].name);
        strcpy(transfs[i].bin, bin);
        
        token = strtok(NULL, " ");
        transfs[i].max = atoi(token);
        transf_availables += transfs[i].max;
    }
    close(config_file);
    
}

// $ ./sdstored config-filename transformations-folder
// $ ./sdstored etc/sdstored.conf bin/sdstore-transformations

int main(int argc, char* argv[]) {

    strcpy(transf_folder, argv[2]);

    if (argc == 3) {
        read_config_file(argv[1]); // 


        // exec_process();

    }
    else { // ERRO
        char invalid[256];
        int invalid_size = sprintf(invalid, "sdstored: wrong number of arguments\n");
        write(1, invalid, invalid_size);
        return 0;
    }
    return 0;
}