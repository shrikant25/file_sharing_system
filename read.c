#include <stdio.h>
#include <string.h>
#include "shared_memory.h"

int main(void){
    /*
    if(argc != 1){
        printf("Invalid arguments\n");
        return -1;
    }
    */ 

    char *block = attach_memory_block(FILENAME, BLOCK_SIZE);
    if(block == NULL) {
        printf("failed to get block");
        return -1;
    }

    printf("Reading \"%s\"\n", block);

    detach_memory_block(block);

    return 0;
}
