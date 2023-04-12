#ifndef PROCESSOR_DB_H
#define PROCESSOR_DB_H

#include "partition.h"

typedef struct db_statements {
    char statement_name[4];
    char *statement;
    int param_count;
}db_statements;

int connect_to_database();
int prepare_statements();
int store_data_in_database(newmsg_data *);
int retrive_data_from_database(newmsg_data *);
int store_commr_into_database(receivers_message *);
int store_comms_into_database(senders_message *);
int retrive_commr_from_database(char *);
int retrive_comms_from_database(senders_message *);
int close_database_connection();

#endif // PROCESSOR_DB_H