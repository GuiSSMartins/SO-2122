#include <unistd.h> /* chamadas ao sistema: defs e decls essenciais */
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h> // modos de abertura 
#include <sys/stat.h>
#include <signal.h>

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

char transf_folder[1024]; // path da pasta onde guardamos os progrmas das transformações


Transf transfs[7]; // Só temos 7 transformações possíveis
int transf_availables = 0;

Process processes[1024]; // processos a serem executados ao mesmo tempo (max 1024 processos)
Process ready_queue[4096]; // processos em fila-de-espera (Podemos guardar no máximo 4096 processos)
int ready_queue_total_size = 0; // Soma total de todos os valores do array anterior
int process_total_size = 0; // Nº total de processos ATIVOS


//         processes[number_process].running = true;

// NAME: encrypt ; decrypt; bcompress ; bdecompress ; gcompress ; gdecompress ; nop
int hash_transf (char* transf_name) {
    switch(transf_name[0]){
        case 'd': //decrypt
            return 0;
           
        case 'e': //encrypt
            return 1;
        case 'b':
            switch(transf_name[1]){
                case 'c': //bcompress
                    return 2;
                default: //bdecompress  
                    return 3;
            }
        case 'g':
            switch(transf_name[1]){
                case 'c': //gcompress
                    return 4;
                default: //gdecompress  
                    return 5;
            }

        case 'n': // nop
            return 6;   
        
        default:
            return -1;
    }
}

// obter nº de bytes de um ficheiro
int get_file_size(int fd){
    return lseek(fd, 0, SEEK_END);
}

void run_process();

int send_reply_message(char* message, int pid, int status);

// "Matar" um processo 'proc-file'
// Também envia informações sobre o nº de bytes do ficheiro de input e de output
void finish_process(pid_t pid) {
    int count = 0;

    int i;
    for (i = 0; i < process_total_size; i++) {
        if (processes[i].fork_pid == pid) break;
        // fork_pid -> pid da execução do processo
    }
    processes[i].running = false; // encontramos o processo que queremos parar
    for (int j = 0; j < processes[i].tp_size && count < processes[i].tp_size; j++) {
        if(processes[i].tp[j].n > 0){
            count++;
            int transf_index = hash_transf(processes[i].tp[j].name);
            transf_availables += processes[i].tp[j].n; // x processos terminaram; x ficaram disponíveis
            transfs[transf_index].running -= processes[i].tp[j].n; // estes x processos já não estão mais a correr
        }
    }

    // Caluclar nº de bytes do ficheiro de input e de output
    int fd_input = open(processes[i].name_input, O_RDONLY);
    int fd_output = open(processes[i].name_output, O_RDONLY);

    int bytes_input = get_file_size(fd_input);
    int bytes_output = get_file_size(fd_output);
    
    close(fd_input);
    close(fd_output);    

    char message[128];
    sprintf(message, "concluded (bytes-input: %d, bytes-output: %d)\n", bytes_input, bytes_output);

    send_reply_message(message, processes[i].client_pid, 0);
    run_process();
}



// Recebe apenas processos com apenas 1 transformação (Justificação do [0])
void exec_transf(int number_process) {
    int child_pid = fork();
    int status;
    int fd_input, fd_output;
    if (child_pid == 0) { // Filho -> Transformação

        int transf_index = hash_transf(processes[number_process].transf_names[0]); // Só uma transformação
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
        strcat(transf_path, transfs[transf_index].name);

        execl(transf_path, transf_path, NULL);

        _exit(0);
    }
    else { // Pai - Servidor
        wait(&status);
        processes[number_process].fork_pid = child_pid;
        finish_process(child_pid);
        // pid do processo-filho
    }
}

// executa várias transformações
void exec_transfs(int index_process) {
    int child_pid;
    int number_transfs = processes[index_process].number_transfs;
    if ((child_pid = fork()) == 0) {
        int status;
        int pipes[number_transfs - 1][2];
        int index, i, j;
        for (i = 0; i < number_transfs - 1; i++) pipe(pipes[i]); // abrir as pipes
        char transf_path[64];
        for (i = 0; i < number_transfs; i++) {
            if (i == 0) { // 1ª Pipe - Apenas ESCRITA
                if (fork() == 0) {
                    index = hash_transf(processes[index_process].transf_names[i]);
                    for (j = 1; j < number_transfs - 1; j++) {
                        close(pipes[j][0]);
                        close(pipes[j][1]);
                    }
                    close(pipes[0][0]);

                    int input_fd = open(processes[index_process].name_input, O_RDONLY);
                    dup2(input_fd, 0);
                    close(input_fd);

                    dup2(pipes[0][1], 1);
                    close(pipes[0][1]);

                    strcpy(transf_path, transf_folder);
                    strcat(transf_path, transfs[index].name);

                    execl(transf_path, transf_path, NULL);
                    _exit(EXIT_FAILURE);
                }
            }
            else if (i == number_transfs - 1) { // Última Pipe - Apenas LEITURA
                if (fork() == 0) {
                    index = hash_transf(processes[index_process].transf_names[i]);
                    for (j = 0; j < number_transfs - 2; j++) {
                        close(pipes[j][0]);
                        close(pipes[j][1]);
                    }
                    close(pipes[i - 1][1]);

                    dup2(pipes[i - 1][0], 0);
                    close(pipes[i - 1][0]);

                    int output_fd = open(processes[index_process].name_output, O_CREAT | O_WRONLY, 0666);
                    dup2(output_fd, 1);
                    close(output_fd);

                    strcpy(transf_path, transf_folder);
                    strcat(transf_path, transfs[index].name);
                    if (fork() == 0) {
                        execl(transf_path, transf_path, NULL);
                    }
                    else {
                        wait(&status);
                        finish_process(child_pid);
                    }
                    _exit(EXIT_FAILURE);
                }
            }
            else { // Outras Pipes - LEITURA e ESCRITA
                if (fork() == 0) {
                    index = hash_transf(processes[index_process].transf_names[i]);
                    for (j = 0; j < number_transfs - 1; j++) {
                        if (j != i - 1 && j != i) {
                            close(pipes[j][0]);
                            close(pipes[j][1]);
                        }
                    }

                    close(pipes[i - 1][1]);
                    dup2(pipes[i - 1][0], 0);
                    close(pipes[i - 1][0]);

                    close(pipes[i][0]);
                    dup2(pipes[i][1], 1);
                    close(pipes[i][1]);

                    strcpy(transf_path, transf_folder);
                    strcat(transf_path, transfs[index].name);
                    execl(transf_path, transf_path, NULL);
                    _exit(EXIT_FAILURE);
                }
            }
        }
        _exit(EXIT_SUCCESS);
    }
    else {
        processes[index_process].fork_pid = child_pid;
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
        int transf_index = hash_transf(token);
        strcpy(transfs[transf_index].name, token);
        
        token = strtok(NULL, " ");
        transfs[transf_index].max = atoi(token);
        transf_availables += transfs[transf_index].max;
    }
    close(config_file);
    
}

// Flag: 0 -> fechar a fifo
//       1 -> para continuar o programa
// pid do request
int send_reply_message(char* message, int pid, int status) {
    Reply reply;
    reply.n_messages = 1;
    reply.status = status; // aqui guaradamos o valor da flag
    strcpy(reply.message[0], message);

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

    return 0;
}

// Flag: 0 -> fechar a fifo
//       1 -> para continuar o programa
// pid do Request do cliente
int send_server_status(int pid) {
    Reply reply;
    reply.n_messages = 0;
    int i, j;

    // status dos processos a rodar no servidor
    for (i = 0; i < process_total_size; i++) {
        if (processes[i].running) {
            char commands[512] = "";
            for (j = 0; j < processes[i].number_transfs; j++) {
                strcat(commands, " ");
                strcat(commands, processes[i].transf_names[j]);
            }

            char process[2048];
            sprintf(process, "task #%d: proc-file %s %s %s\n", i + 1, processes[i].name_input, processes[i].name_output, commands);
            strcpy(reply.message[reply.n_messages], process);
            reply.n_messages++;
        }
    }

    //status das 7 transformações
    for (i = 0; i < 7; i++) {
        char transfs_status[128];
        sprintf(transfs_status, "transf %s: %d/%d (running/max)\n", transfs[i].name, transfs[i].running, transfs[i].max);
        strcpy(reply.message[reply.n_messages], transfs_status);
        reply.n_messages++;
    }
    
    int server_to_client_fifo;
    char path_server_to_client_fifo[128];
    sprintf(path_server_to_client_fifo, "namedpipe/%d", pid);
    if ((server_to_client_fifo = open(path_server_to_client_fifo, O_WRONLY)) == -1) {
        char invalid_fifo[256];
        int invalid_fifo_size = sprintf(invalid_fifo, "sdstore: couldn't open server-to-client FIFO\n");
        write(1, invalid_fifo, invalid_fifo_size);
        return 1;
    }

    reply.status = 0;  // flag para fechar a fifo

    write(server_to_client_fifo, &reply, sizeof(Reply));
    close(server_to_client_fifo);
    unlink(path_server_to_client_fifo); // "matar" o ficheiro da fifo

    return 0;
}

// verificar se o nome da tarnsformação recebida é uma tarnsformação válida
int verify_transf_name(char* transf_name) {
    int transf_index = hash_transf(transf_name);
    if(transf_index != -1){
        if (strcmp(transfs[transf_index].name, transf_name) == 0) return 1;
    }
    return 0;
}

// verificar se nomes das tarnsformações pedidas no comando são tarnsformações válidas
int verify_transfs_names(char transfs_names[64][64], int number_transf, int* index) {
    int valid = 1;
    int i;
    for (i = 0; i < number_transf && valid; i++) {
        if (!verify_transf_name(transfs_names[i])) valid = 0;
    }
    *index = i - 1; // índice da última transformação válida
    return valid;
}


// Colocar informações sobre cada uma das transformações
// Devolve o valor do tp_size -> nº de TIPOS de transformações num processo
int add_info_tprocess(char transfs_names[64][64], int number_transfs, TProcess tprocess[7]) {
    int tp_size = 0;
    int i;
    for (i = 0; i < number_transfs; i++) {
        int transf_index = hash_transf(transfs_names[i]);
        if (tprocess[transf_index].n == 0) {
            tp_size++;
        }
        strcpy(tprocess[transf_index].name, transfs_names[i]);
        tprocess[transf_index].n++;
    }
    return tp_size;
}


// verifica se a quantidade de transformações é válida para o máximo recebido
int validate_transf(int index, int number) {
    if (number > transfs[index].max) return 0;
   
    return (transfs[index].running + number <= transfs[index].max ? 1 : 0);
    
}

// verifica se a quantidade de transformações é válida para o máximo recebido
int validate_transfs(TProcess tprocess[7], int tp_size) {
    int valid = 1;
    int count = 0;
    int i;
    for (i = 0; i < 7 && valid && count < tp_size; i++) {
        int transf_index = hash_transf(tprocess[i].name);
        if(transf_index != -1){
            count++;
            if(!validate_transf(transf_index, tprocess[i].n)) valid = 0;
        }
        
    }
    return valid;
}

// atualizar o 'running' de cada uma das tarnsformações do processo 
void save_transfs(TProcess tprocess[7], int tp_size) {
    int i;
    int count = 0;
    for (i = 0; i < 7 && count < tp_size; i++) {
        int transf_index = hash_transf(tprocess[i].name);
        if(transf_index != -1){
            count++;
            transfs[transf_index].running += tprocess[i].n;
        }
    }
}

// inicializar as várias variáveis da estrtura de um tprocess
void init_tprocess(TProcess tprocess[7]){
    for(int i = 0; i < 7; i++){
        tprocess[i].n = 0;
        strcpy(tprocess[i].name,"");
    }
}

// Executar um processo ou conjunto de processos
void run_process() {
    int i;
    for (i = 0; ready_queue_total_size > 0; i++) {
        if (ready_queue[i].ready && ready_queue[i].number_transfs <= transf_availables) {
            if (validate_transfs( ready_queue[i].tp, ready_queue[i].tp_size) ) {
                
                save_transfs(ready_queue[i].tp, ready_queue[i].tp_size);
                transf_availables -= ready_queue[i].number_transfs;
                ready_queue[i].ready = false;
                ready_queue_total_size--;
                Process process = ready_queue[i];
                process.running = true;
                processes[process_total_size] = process;
                process_total_size++;
                send_reply_message("processing\n", processes[process_total_size - 1].client_pid, 1);
                
                if (i == 0) exec_transf(process_total_size - 1);
                else exec_transfs(process_total_size - 1);
            }
        }
    }
}

void close_handler(int signum) {
    while(ready_queue_total_size > 0){
        run_process();
    }
    unlink(CLIENT_TO_SERVER_FIFO);
    exit(0);
}


// $ ./sdstored config-filename transformations-folder
// $ ./sdstored etc/sdstored.conf bin/sdstore-transformations

int main(int argc, char* argv[]) {
    server_pid = getpid();

    signal(SIGINT, close_handler);
    signal(SIGTERM, close_handler);

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

                // status
                if (request.n_messages == 1 && strcmp("status", request.message[0]) == 0) {
                    send_server_status(request.pid);
                }
                // proc-file
                else if (request.n_messages > 3 && strcmp("proc-file", request.message[0]) == 0) { // Transformação válida
                    
                    send_reply_message("pending\n", request.pid, 1); // flag: 1 -> há conteúdo do cliente para ler
                    char transfs_names_process[64][64]; // nomes das várias transformações

                    int last_valid_transf_index; // índice da última transformação válida

                    // Não tem a prioridade
                    for (int i = 3; i < request.n_messages; i++) { // só a aprtir do índice 3, temos as strings das transformações
                        strcpy(transfs_names_process[i - 3], request.message[i]);
                    }
                    // Só fazemos todas as transformações apenas se todos os nomes delas forem válidas
                    if (verify_transfs_names(transfs_names_process, request.n_messages - 3, &last_valid_transf_index)) { // sem os argumentos de inicio do store
                    
                        TProcess tprocess[7]; // tuplo dos processos
                        init_tprocess(tprocess);

                        // Nº total de transformações = argc - 3

                        int tp_size = add_info_tprocess(transfs_names_process, request.n_messages-3, tprocess);
                        if (validate_transfs(tprocess, tp_size)) {
                            // o nº de transformações está adequado para o máximo que podmeos ter
                            Process process;
                            int i;
                            for (i = 3; i < request.n_messages; i++) {
                                strcpy(process.transf_names[i - 3], request.message[i]);
                            }
                            process.number_transfs = request.n_messages - 3;
                            process.client_pid = request.pid;
                            process.running = false;
                            process.ready = true;
                            ready_queue_total_size++;
                            strcpy(process.name_input, request.message[1]);
                            strcpy(process.name_output, request.message[2]);
                            process.tp_size = tp_size;
                            for (i = 0; i < process.tp_size; i++) process.tp[i] = tprocess[i];

                            ready_queue[ready_queue_total_size -1] = process;
                            
                            run_process();
                        }
                        else send_reply_message("exceeded maximum number of transformations set by 'config-file'\n", request.pid, 0);

                    }
                    else {
                        char message[1024];
                        sprintf(message, "invalid transformation: %s\n", request.message[last_valid_transf_index + 3]);
                        send_reply_message(message, request.pid, 0); // fechámos a pipe
                    }
                }
            }

            close(client_to_server_fifo);
        }

    }
    else { // ERRO
        char invalid[256];
        int invalid_size = sprintf(invalid, "sdstored: wrong number of arguments\n");
        write(1, invalid, invalid_size);
        return 0;
    }

    return 0;
}