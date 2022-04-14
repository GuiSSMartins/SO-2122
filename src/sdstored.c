#include <unistd.h> /* chamadas ao sistema: defs e decls essenciais */
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h> // modos de abertura 

#include <stdio.h>
#include <string.h>
#include <stdlib.h> // sizeof

// SERVIDOR

// $ ./sdstored config-filename transformations-folder
// $ ./sdstored etc/sdstored.conf bin/sdstore-transformations

// nop = 0; bcompress = 1; bdecompress = 2; gcompress = 3; gdecompress = 4; encrypt = 5; decrypt = 6;
typedef struct transf {
    char name[16];
    char bin[32];
    int running;
    int max;
} Transf; // Transformação

Transf transfs[7];

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
    }
    close(config_file);
    
}


int main(int argc, char* argv[]) {

    if (argc == 3) {
        read_config_file(argv[1]);
    }
    else { // ERRO
        char invalid[256];
        int invalid_size = sprintf(invalid, "sdstored: wrong number of arguments\n");
        write(1, invalid, invalid_size);
        return 0;
    }
    return 0;
}