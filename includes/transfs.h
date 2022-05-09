#ifndef __TRANSFS__
#define __TRANSFS__

// NAME: nop ; bcompress ; bdecompress ; gcompress ; gdecompress ; encrypt ; decrypt
typedef struct transf {
    char name[16]; // nome da transformação
    char bin[32]; // path do executável da transformação
    int running; // quantas transformações deste tipo estão a a serem processadas
    int max; // nº máximo de transformações que podemos fazer deste tipo
} Transf; // Transformação

#endif