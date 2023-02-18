#ifndef SENDER_H
#define SENDER_H
#include <semaphore.h>


int close_sem_locks();
int initialize_locks();
int get_shared_memeory();
int run_sender();
int send_data();
int evaluate_and_take_action(char *, int *);
int read_message(char *, int *, int *);
int send_message(char *, int, int *);
int get_data_from_processor(char *, int *);
int send_data_over_network(unsigned int, char *);

typedef struct datablocks {     
    char *datas_block;
    char *comms_block;
}datablocks;

typedef struct semlocks {
    sem_t *sem_lock_datas;
    sem_t *sem_lock_comms; 
}semlocks;



#endif // SENDER_H