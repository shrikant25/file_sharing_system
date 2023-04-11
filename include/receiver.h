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

int run_receiver();
void accept_connection();
void remove_from_list(int);
void add_to_list(int);
void create_socket();
void make_nonblocking(int);
int send_to_processor(newmsg_data *);
int read_message_from_processor(char *);
int send_message_to_processor(receivers_message *);

#endif //RCEIVER_H
