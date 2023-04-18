#ifndef PROCESSOR_S_H
#define PROCESSOR_S_H

#include <semaphore.h>
#include "partition.h"

int run_process();
int communicate_with_sender();
int give_data_to_sender();
void store_log(char *);
int retrive_comms_from_database(char *);
int store_comms_into_database(char *);
int retrive_data_from_database(char *); 
int prepare_statements();
int connect_to_database();

typedef struct db_statements {
    char statement_name[20];
    char *statement;
    int param_count;
}db_statements;


typedef struct datablocks {
    char *datas_block;
    char *comms_block;
}datablocks;

typedef struct semlocks {
    sem_t *sem_lock_datas;
    sem_t *sem_lock_comms; 
}semlocks;

#endif // PROCESSOR_H 