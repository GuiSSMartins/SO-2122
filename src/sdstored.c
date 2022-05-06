#include <unistd.h> /* chamadas ao sistema: defs e decls essenciais */
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h> // modos de abertura 

#include <stdio.h>
#include <string.h>
#include <stdlib.h> // sizeof
#include <stdbool.h>

#include "../includes/request.h"
#include "../includes/reply.h"
#include "../includes/transfs.h"
#include "../includes/tprocess.h"
#include "../includes/process.h"


// SERVIDOR

// $ ./sdstored config-filename transformations-folder
// $ ./sdstored etc/sdstored.conf bin/sdstore-transformations

int server_pid; // PID do Servidor

#define CLIENT_TO_SERVER_FIFO "namedpipe/client_to_server_fifo" // Path para guadra fifo do cliente para o servidor

char* transf_folder[1024]; // path da pasta onde guardamos os progrmas das transformações


Transf transfs[7]; // Só temos 7 transformações possíveis
int transf_availables = 0;

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

void read_config_file(char* path) { // Lê do ficheiro config os dados sobre a transformação
    char buffer[32];
    int config_file = open(path, O_RDONLY, 0666);
    if (config_file < 0) {
        char invalid_config_file[256];
        int invalid_config_file_size = sprintf(invalid_config_file, "sdstored: couldn't open file 'config-file'");
        write(1, invalid_config_file, invalid_config_file_size );

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

// Flag: 0 -> fechar a fifo
//       1 -> para continuar o programa
void send_reply_message(char* message, int pid, int status) {
    Reply reply;
    reply.argc = 1;
    reply.status = status;
    strcpy(reply.argv[0], message);
    int server_to_client_fifo;
    char path_server_to_client_fifo[128];
    sprintf(path_server_to_client_fifo, "namedpipe/%d", pid);
    if ((server_to_client_fifo = open(path_server_to_client_fifo, O_WRONLY)) == -1) {
        char invalid_fifo[256];
        int invalid_fifo_size = sprintf(invalid_fifo, "sdstore: couldn't open server-to-client FIFO\n");
        write(1, invalid_fifo, invalid_fifo_size);
        return 1;
    }
    write(server_to_client_fifo, &reply, sizeof(Reply));
    close(server_to_client_fifo);
    if (!status) unlink(path_server_to_client_fifo);
}

int verify_transf(char* transf_name) {
    int valid = 0;
    for (int i = 0; i < 7 && !valid; i++)
        valid |= (strcmp(transf_name, transfs[i].name) == 0);
    return valid;
}

// transformações de um processo
int verify_transfs(char transfs_names[64][64], int number_transf, int* index) {
    int valid = 1;
    int i;
    for (i = 0; i < number_transf && valid; i++) {
        valid &= verify_transf(transfs_names[i]);
    }
    *index = i - 1; // índice da última transformação válida
    return valid;
}

// $ ./sdstored config-filename transformations-folder
// $ ./sdstored etc/sdstored.conf bin/sdstore-transformations

int main(int argc, char* argv[]) {
    server_pid = getpid();

    if (argc == 3) {
        strcpy(transf_folder, argv[2]);

        read_config_file(argv[1]);

        if (mkfifo(CLIENT_TO_SERVER_FIFO, 0666) == -1) {
            char invalid_fifo[256];
            int invalid_fifo_size = sprintf(invalid_fifo, "sdstored: couldn't create client-to-server FIFO\n");
            write(1, invalid_fifo, invalid_fifo_size);
            return 1;
        }

        Request request;
        while (1) {
            int client_to_server_fifo = open(CLIENT_TO_SERVER_FIFO, O_RDONLY, 0666);
            while (read(client_to_server_fifo, &request, sizeof(Request)) > 0) {

                if (request.n_args > 3 && strcmp("proc-file", request.argv[0]) == 0) { // Transformação válida
                    send_reply_message("pending\n", request.pid, 1); // flag: 1 -> há conteúdo do cliente para ler
                    char transfs_names[64][64];

                    int last_valid_transf_index; // índice da última transformação válida

                    // Não tem a prioridade
                    for (int i = 3; i < request.n_args; i++) { // só a aprtir do índice 3, temos as strings das transformações
                        strcpy(transfs_names[i - 3], request.argv[i]);
                    }
                    if (verify_transfs(transfs_names, request.n_args - 3, &last_valid_transf_index)) { // sem os argumentos de inicio do store
                    
                    }
                    else {
                        char message[1024];
                        sprintf(message, "invalid transformation: %s\n", request.argv[last_valid_transf_index + 3]);
                        send_message(message, request.pid, 0); // fechámos a pipe
                    }
                }

            }

        }

    }
    else { // ERRO
        char invalid[256];
        int invalid_size = sprintf(invalid, "sdstored: wrong number of arguments\n");
        write(1, invalid, invalid_size);
        return 0;
    }


}