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
    sem_t *sem_lock_sigr;
}semlocks;

typedef struct server_info{
    unsigned short int port;
    unsigned int maxevents;
    unsigned int servsoc_fd;
    unsigned int epoll_fd;
}server_info;

typedef struct db_statements {
    char statement_name[20];
    char *statement;
    int param_count;
}db_statements;


#define statement_count 1

db_statements dbs[statement_count] = {
    { 
      .statement_name = "r_storelog",  
      .statement = "INSERT INTO logs (log) VALUES ($1)",
      .param_count = 1
    }
};

int connect_to_database();
int prepare_statements();
void store_log(char *logtext);
int run_receiver();
int accept_connection();
int remove_from_list(int);
int add_to_list(int);
int create_socket();
int make_nonblocking(int);
int send_to_processor(newmsg_data *);
int read_message_from_processor(char *);
int send_message_to_processor(receivers_message *);


#endif //RCEIVER_H
