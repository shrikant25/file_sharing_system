#ifndef SENDER_H
#define SENDER_H
#include <semaphore.h>
#include "partition.h"


int create_connection(unsigned short int, unsigned int); 
int evaluate_and_take_action(senders_message *);
int get_message_from_processor(senders_message *); 
int get_data_from_processor(newmsg_data *);
int send_data_over_network(newmsg_data *); 
int send_message_to_processor(int, int, int); 
int run_sender(); 

typedef struct datablocks {     
    char *datas_block;
    char *comms_block;
}datablocks;

typedef struct semlocks {
    sem_t *sem_lock_datas;
    sem_t *sem_lock_comms; 
}semlocks;



#endif // SENDER_H