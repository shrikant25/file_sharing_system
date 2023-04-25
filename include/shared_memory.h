#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <semaphore.h>

typedef struct datablocks {
    unsigned int key;
    char *var;
}datablocks;


typedef struct semlocks {
  char key[20];
  sem_t *var;
}semlocks;

#define FILENAME_R "processor_r"
#define FILENAME_S "processor_s"

char *attach_memory_block(char *, int, unsigned char);
int detach_memory_block(char *);
int destroy_memory_block(char *,unsigned char);

#endif //SHARED_MEMORY_H