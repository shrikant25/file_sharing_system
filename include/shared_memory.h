#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#define SEM_SHM_LOCK "shm_shm_lock"

char * attach_memory_block(char *filename, int size);
bool detach_memory_block(char *block);
bool destroy_memory_block(char *filename);

#define FILENAME "p1"

#endif