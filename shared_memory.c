#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/shm.h>
#include "shared_memory.h"


int get_shared_block(char *filename, int size, unsigned char project_id) 
{
    key_t key;

    key = ftok(filename, project_id);
    if (key == -1){
        return -1;
    }

    return shmget(key, size, 0644 | IPC_CREAT);
}


char *attach_memory_block(char *filename, int size, unsigned char project_id) 
{ 
    char *result;
    int shared_block_id = get_shared_block(filename, size, project_id);

    if (shared_block_id == -1) {
        return NULL;
    }

    result = (char *)shmat(shared_block_id, NULL, 0);
    if (result == (char *)-1) {
        return NULL;
    }

    return result;
}


int detach_memory_block(char *block) 
{
    return (shmdt(block) != -1);
}


int destroy_memory_block(char *filename, unsigned char project_id)
 {
    int shared_block_id = get_shared_block(filename, 0, project_id);

    if (shared_block_id == -1) {
        return -1;
    }

    return (shmctl(shared_block_id, IPC_RMID, NULL) != -1);
}