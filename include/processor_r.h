#ifndef PROCESSOR_R_H
#define PROCESSOR_R_H

#include <semaphore.h>
#include "partition.h"


int connect_to_database();
int prepare_statements();
void store_log(char *);
int run_process();
int retrive_commr_from_database(char *);
int store_data_in_database(newmsg_data *);
int store_commr_into_database(receivers_message *);
int communicate_with_receiver();
int get_data_from_receiver();

typedef struct db_statements {
    char statement_name[20];
    char *statement;
    int param_count;
}db_statements;


typedef struct datablocks {
    char *datar_block;
    char *commr_block;
}datablocks;

typedef struct semlocks {
    sem_t *sem_lock_datar;
    sem_t *sem_lock_commr;
    sem_t *sem_lock_sigdr;
    sem_t *sem_lock_sigmr;
}semlocks;

#endif // PROCESSOR_H 