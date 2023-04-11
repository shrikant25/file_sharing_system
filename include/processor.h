#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <semaphore.h>

int run_process();
int communicate_with_sender();
int communicate_with_receiver();
int give_data_to_sender();
int get_data_from_receiver();

typedef struct datablocks {

    char *datar_block;
    char *commr_block;
    char *datas_block;
    char *comms_block;

}datablocks;

typedef struct semlocks {

    sem_t *sem_lock_datar;
    sem_t *sem_lock_commr;
    sem_t *sem_lock_datas;
    sem_t *sem_lock_comms; 

}semlocks;

#endif // PROCESSOR_H 