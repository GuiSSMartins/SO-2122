#include <unistd.h> /* chamadas ao sistema: defs e decls essenciais */
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h> // modos de abertura 

#include <stdio.h>
#include <string.h>
#include <stdlib.h> // sizeof
#include <ctype.h>

// CLIENTE

// ./sdstore proc-file <priority> samples/file-a outputs/file-a-output bcompress nop gcompress encrypt nop

int main(int argc, char* argv[]) {
    int index=1;
    char buffer[1024];
    if (argc < 2) { // ./sdstore
        char status[128];
        char example[128];
        int status_size = sprintf(status, "%s status\n", argv[0]);
        int example_size = sprintf(example,
                    "%s proc-file priority input-filename output-filename transformation-id-1 transformation-id-2 ...\n",
                    argv[0]);
        write(1, status, status_size);
        write(1, example, example_size);
    }
    else if (strcmp(argv[1],"status")==0 && argc == 2) {

        // sprintf(transf1, "transf nop: %d/%d (running/max)\n", config.atual[0], config.max[0]);


    }
    else if (strcmp(argv[2],"proc-file")==0) { // Pipes COM Nome - Comunicação para o servidor
        if ( argc < 5) {
            char invalid[256];
            int invalid_size = sprintf(invalid, "sdstore: wrong number of arguments\n");
            write(1, invalid, invalid_size);
            return 0;
        }

        else { // Pipes COM Nome - Comunicação para o servidor
            // if(isdigit(argv[2][0]))

            // strcat(buffer, );
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