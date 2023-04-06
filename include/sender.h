#ifndef SENDER_H
#define SENDER_H
#include <semaphore.h>


int run_sender();
int send_data();
int evaluate_and_take_action(char *);
int read_message(char *);
int send_message(char *, int);
int get_data_from_processor(int *, char *, int *);
int send_data_over_network(unsigned int, char *, int);

typedef struct datablocks {     
    char *datas_block;
    char *comms_block;
}datablocks;

typedef struct semlocks {
    sem_t *sem_lock_datas;
    sem_t *sem_lock_comms; 
}semlocks;



#endif // SENDER_H