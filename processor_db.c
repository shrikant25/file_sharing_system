#include <libpq-fe.h>
#include "partition.h"
#include "processor_db.h"

database_info dbinf;
//if any error occurs try to mitigate it here only
//if mitigation fails then return -1
int connect_to_database() {
   
    dbinf.connection = PQconnectdb("user=shrikant dbname=shrikant");

    if (PQstatus(dbinf.connection) == CONNECTION_BAD) {
        fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(dbinf.connection));
        // mitigate
        //if fails then return -1
    }

    return 0;
}


int prepare_statments() {
    
    int i;

    for(i = 0; i<dbinf.statement_count; i++){

        PGresult* res = PQprepare(dbinf.connection, dbinf.statement_names[i], 
                                    dbinf.statements[i], dbinf.param_count[i], NULL);

        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            printf("Preparation of statement failed: %s\n", PQerrorMessage(dbinf.connection));
        }

        PQclear(res);
    }

    return 0;
}


int store_data_in_database(char *data) {

    const char param_values = {data};
    PGresult* res = PQexecPrepared(dbinf.connection, dbinf.statement_names[i], 1, param_values, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(dbinf.connection));
        
    }

    PQclear(res);

    return 0;
}


int retrive_data_from_database(char *) {

}


int retrive_comms_from_database(char *) {

}


int retrive_commr_from_database(char *) {

}


// if first byte is denotes values 1, then it is data containing fd,ip and port
int store_comms_into_database(char *data) {

    char fd[2];
    char ip_addr[4];
    char *ptr = data;

    if (*ptr == 1){

        ptr++;
        memcpy(fd, *ptr, 2);   

        ptr += 2;
        memcpy(ip_addr, *ptr, 4);

        const char param_values = {fd, ip_addr};
        
        PGresult* res = PQexecPrepared(dbinf.connection, dbinf.statement_names[2], dbinf.param_count[2], param_values, NULL, NULL, 0);

    }

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(dbinf.connection));
    }

    PQclear(res);

    return 0;

}


int store_commr_into_database(char *) {

    char msg_id[16];
    char msg_status[1]; 
    char *ptr = data;

    if (*ptr == 1){

        ptr++;
        memcpy(msg_id, *ptr, 16);   

        ptr += 16;
        memcpy(msg_status, *ptr, 1);

        const char param_values = {msg_id, msg_status};
        
        PGresult* res = PQexecPrepared(dbinf.connection, dbinf.statement_names[4], dbinf.param_count[4], param_values, NULL, NULL, 0);

    }

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(dbinf.connection));
    }

    PQclear(res);

    return 0;

}


int close_database_connection() {   
    
    int status = 0;
    status = PQfinish(dbinf.connection);
    return status;

}