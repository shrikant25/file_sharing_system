#ifndef PROCESSOR_DB_H
#define PROCESSOR_DB_H

#include <libpq-fe.h>

int close_database_connection();
int connect_to_database();
int store_data_in_database(char *);
int retrive_data_from_database(char *);
int retrive_comm_from_database(char *);
int store_comm_from_database(char *);

typedef struct database_info{
    PGconn *connection;
    struct prepared_statement{
        char *ps_strdata_db = "store_data_in_database";
        char *ps_rtrdata_db = "retrive_data_from_database";
        char *ps_strcomm_db = "store_comm_from_database";
        char *ps_rtrcomm_db = "retrive_comm_from_database";
    }prpd_stmts;
}database_info;


#endif // PROCESSOR_DB_H