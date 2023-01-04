#include <stdio.h>
#include "shared_memory.h"

int main(int argc, char *argv[]) {


    if(destroy_memory_block(FILENAME)) {
        printf("Destroyed block  %s\n", FILENAME);
    }
    else{
             printf("failed to destroy block  %s\n", FILENAME);
    }

    return 0;
}