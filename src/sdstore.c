#include <unistd.h> /* chamadas ao sistema: defs e decls essenciais */
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h> // modos de abertura 
#include <sys/stat.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h> // sizeof
#include <ctype.h>

#include "../includes/request.h"
#include "../includes/reply.h"

// CLIENTE
// ./sdstore proc-file <priority> samples/file-a outputs/file-a-output bcompress nop gcompress encrypt nop

// Neste momento, não temos a prioridade

#define CLIENT_TO_SERVER_FIFO "namedpipe/client_to_server_fifo" // Path para guadra fifo do cliente para o servidor

char path_server_to_client_fifo[128]; // path para guadar Fifo do servidor aos (!!!) clientes!


void create_request(int client_to_server_fifo, int argc, char** argv) {
    Request request;
    request.n_args = argc - 1; // (proc-file) nº de transformações
    request.pid = getpid();
    for (int i = 1; i < argc; i++) {
        strcpy(request.argv[i - 1], argv[i]);
    }
    write(client_to_server_fifo, &request, sizeof(Request));
}

void reply() {
    Reply reply;
    int server_to_client_fifo = open(path_server_to_client_fifo, O_RDONLY);
    while (1) {
        while (read(server_to_client_fifo, &reply, sizeof(Reply)) > 0) {
            int i;
            for (i = 0; i < reply.argc; i++) {
                write(1, reply.argv[i], strlen(reply.argv[i])); // escrever o estado de cada processo no ecrã
            }
            if (!reply.status) { // Fechar o programa
                close(server_to_client_fifo);
                exit(0);
            }
        }
    }
    close(server_to_client_fifo);
}

void close_handler(int signum) {
    unlink(path_server_to_client_fifo); // "destruir" o ficheiro da pipe // não recebemos mais informações do servidor 
    exit(0);
}

int main(int argc, char* argv[]) {

    sprintf(path_server_to_client_fifo, "namedpipe/%d", (int)getpid());

    signal(SIGINT, close_handler);
    signal(SIGTERM, close_handler);

    if ((mkfifo(path_server_to_client_fifo, 0666)) == -1) {
        char invalid_fifo[256];
        int invalid_fifo_size = sprintf(invalid_fifo, "sdstore: couldn't create server-to-client FIFO\n");
        write(1, invalid_fifo, invalid_fifo_size);
        return 1;
    }


    if (argc < 2) { // ./sdstore
        char status[128];
        char example[128];
        int status_size = sprintf(status, "%s status\n", argv[0]);
        int example_size = sprintf(example, "%s proc-file priority input-filename output-filename transformation-id-1 transformation-id-2 ...\n", argv[0]);
        write(1, status, status_size);
        write(1, example, example_size);
    }

    else if ((strcmp(argv[1],"status")==0) || (strcmp(argv[1],"proc-file")==0)) {

        if ((strcmp(argv[1], "status") == 0) && argc > 2) {
            char invalid_status[256];
            int invalid_status_size = sprintf(invalid_status, "sdstore: invalid status command\n");
            write(1, invalid_status, invalid_status_size);
            return 0;
        }
        else if ((strcmp(argv[1], "proc-file") == 0) && argc < 5) {
            char invalid_proc_file[256];
            int invalid_proc_file_size = sprintf(invalid_proc_file, "sdstore: wrong number of arguments\n");
            write(1, invalid_proc_file, invalid_proc_file_size);
            return 0;
        }
        else { // Pipes COM Nome - Comunicação para o servidor
            
            int client_to_server_fifo = open(CLIENT_TO_SERVER_FIFO, O_WRONLY);
            create_request(client_to_server_fifo, argc, argv);
            close(client_to_server_fifo);
            reply();
        }

    }
    else { // Erro nos argumentos
        char invalid[256];
        int invalid_size = sprintf(invalid, "sdstore: invalid command\n");
        write(1, invalid, invalid_size);
        return 0;
    }
    return 0;
}