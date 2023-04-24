#ifndef SENDER_H
#define SENDER_H
#include <semaphore.h>
#include "partition.h"


int create_connection(unsigned short int, unsigned int); 
int get_message_from_processor(char *); 
int get_data_from_processor(send_message *);
int send_message_to_processor(int, void *); 
int run_sender(); 

typedef struct datablocks {     
    char *datas_block;
    char *comms_block;
}datablocks;

typedef struct semlocks {
    sem_t *sem_lock_datas;
    sem_t *sem_lock_comms; 
    sem_t *sem_lock_sigs;
    sem_t *sem_lock_sigps;
}semlocks;

typedef struct db_statements {
    char statement_name[20];
    char *statement;
    int param_count;
}db_statements;

int connect_to_database();
int prepare_statements();

#define statement_count 1

db_statements dbs[statement_count] = {
    { 
      .statement_name = "s_storelog",  
      .statement = "INSERT INTO logs (log) VALUES ($1)",
      .param_count = 1
    }
};

#endif // SENDER_H