#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <semaphore.h>

int get_data_from_receiver(int *);
int give_data_to_receiver(int *);
int communicate_with_receiver(int *, int *);
int communicate_with_sender(int *, int *);
int run_process();
int open_sem_locks();
int close_sem_locks();
int get_shared_memory();
int detach_shared_memory();

typedef struct datablocks{
char * datar_block;
char * commr_block;
char * datas_block;
char * comms_block;
}datablocks;

typedef struct semlocks{
sem_t *sem_lock_datar;
sem_t *sem_lock_commr;
sem_t *sem_lock_datas;
sem_t *sem_lock_comms; 
}semlocks;

extern datablocks dblks;
extern semlocks slks;

#endif // PROCESSOR_H 