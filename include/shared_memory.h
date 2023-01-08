#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#define SEM_SHMP_LOCK "sem_shmp_lock"
#define SEM_SHMC_LOCK "sem_shmc_lock"

char * attach_memory_block(char *filename, int size);
int detach_memory_block(char *block);
int destroy_memory_block(char *filename);

#define FILENAME "p1"

#endif