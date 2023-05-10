#ifndef I_RECEIVER_H
#define I_RECEIVER_H

#include "partition.h"
#define MESSAGE_SIZE 22


typedef struct server_info{
    unsigned short int port;
    unsigned int servsoc_fd;
    unsigned long ipaddress;
}server_info;

typedef struct db_statements {
    char *statement_name;
    char *statement;
    int param_count;
}db_statements;


#define statement_count 1

db_statements dbs[statement_count] = {
    { 
      .statement_name = "ir_storelog",  
      .statement = "INSERT INTO logs (log) VALUES ($1);",
      .param_count = 1
    }
};

int connect_to_database();
int prepare_statements();
void store_log(char *logtext);

#endif