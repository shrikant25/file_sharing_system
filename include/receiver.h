#ifndef RECEIVER_H
#define RECEIVER_H
#include <semaphore.h>
#include "partition.h"

typedef struct datablocks {     
    char *datar_block;
    char *commr_block;
}datablocks;


typedef struct semlocks {
    sem_t *sem_lock_datar;
    sem_t *sem_lock_commr; 
}semlocks;


typedef struct server_info{

    unsigned short int port;
    unsigned int maxevents;
    unsigned int servsoc_fd;
    unsigned int epoll_fd;

}server_info;


void make_nonblocking(int);
void create_socket();
void create_epoll();
void add_to_list(int);
void remove_from_list(int); 
void accept_connection(); 
void read_socket(struct epoll_event);
int send_to_processor(nmdata *);
int read_message_from_processor(char *);
int evaluate_and_perform(char *);
int send_message_to_processor(receivers_msg *);
int run_receiver();

#endif //RCEIVER_H
