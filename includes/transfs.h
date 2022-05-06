// NAME: nop ; bcompress ; bdecompress ; gcompress ; gdecompress ; encrypt ; decrypt
typedef struct transf {
    char name[16];
    char bin[32];
    int running;
    int max;
} Transf; // Transformação