#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#define SEM_LOCK "semlock"

char * attach_memory_block(char *filename, int size);
int detach_memory_block(char *block);
int destroy_memory_block(char *filename);

#define FILENAME "p1"

#endif