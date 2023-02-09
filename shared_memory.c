#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "shared_memory.h"

#define IPC_RESULT_ERROR (-1)

static int get_shared_block(char *filename, int size, unsigned char project_id) {

    key_t key;

    key = ftok(filename, project_id);
    if (key == IPC_RESULT_ERROR){
        return IPC_RESULT_ERROR;
    }

    return shmget(key, size, 0644 | IPC_CREAT);
}

char *attach_memory_block(char *filename, int size, unsigned char project_id) {
 
    int shared_block_id = get_shared_block(filename, size, project_id);
    char *result;

    if (shared_block_id == IPC_RESULT_ERROR) {
        return NULL;
    }

    result = shmat(shared_block_id, NULL, 0);
    if (result == (char *)IPC_RESULT_ERROR) {
        return NULL;
    }

    return result;

}

int detach_memory_block(char *block) {
    return (shmdt(block) != IPC_RESULT_ERROR);
}

int destroy_memory_block(char *filename) {

    int shared_block_id = get_shared_block(filename, 0);

    if (shared_block_id == IPC_RESULT_ERROR) {
        return -1;
    }

    return (shmctl(shared_block_id, IPC_RMID, NULL) != IPC_RESULT_ERROR);
}