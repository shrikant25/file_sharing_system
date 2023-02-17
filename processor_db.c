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

    PGresult *res = NULL;
    const char param_values[dbinf.param_count[0]] = {data};

    res = PQexecPrepared(dbinf.connection, dbinf.statement_names[0], 
                                    param_count[0], param_values, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(dbinf.connection));
        
    }

    PQclear(res);

    return 0;
}


int retrive_data_from_database(char *data) {

    int row_count;
    PGresult *res = NULL;

    res = PQexecPrepared(dbinf.connection, dbinf.statement_names[1], param_count[1], 
                                    NULL, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(dbinf.connection));
        return 1;
    }    

    row_count = PQntuples(res);
    if (row_count > 0) {
        memcpy(data, PQgetvalue(res, 0, 0), PQgetlength(res, 0, 0));
        memcpy(data+2, PQgetvalue(res, 0, 1), PQgetlength(res, 0, 1));
        PQclear(res);
        return 0;
    }

    PQclear(res);
    return 1;

    
}


// if first byte is denotes values 1, then it is data containing fd,ip and port
int store_commr_into_database(char *data) {

    char fd[2];
    char ip_addr[4];
    unsigned char *ptr = data;
    PGresult *res = NULL;

    if (*ptr == 1) {

        ptr++;
        memcpy(fd, *ptr, 2);   

        ptr += 2;
        memcpy(ip_addr, *ptr, 4);

        const char param_values[dbinf.param_count[0]] = {fd, ip_addr};
        
        res = PQexecPrepared(dbinf.connection, dbinf.statement_names[2], dbinf.param_count[2], param_values, NULL, NULL, 0);

    }

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(dbinf.connection));
    }

    PQclear(res);

    return 0;

}


int store_comms_into_database(char *data) {

    char msg_id[16];
    char msg_status[1]; 
    unsigned char *ptr = data;
    PGresult* res = NULL;

    if (*ptr == 1) {

        ptr++;
        memcpy(msg_id, *ptr, 16);   

        ptr += 16;
        memcpy(msg_status, *ptr, 1);

        const char param_values[dbinf.param_count[3]] = {msg_id, msg_status};
        
        res = PQexecPrepared(dbinf.connection, dbinf.statement_names[3], dbinf.param_count[3], 
                                        param_values, NULL, NULL, 0);

    }

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(dbinf.connection));
    }

    PQclear(res);

    return 0;

}


int retrive_commr_from_database(char *data) {

    int row_count = 0;
    PGresult *res = NULL;

    res = PQexecPrepared(dbinf.connection, dbinf.statement_names[4], param_count[4],
                                    NULL, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(dbinf.connection));
        return 1;
    }    

    row_count = PQntuples(res);
    if (row_count > 0) {
        memcpy(data, PQgetvalue(res, 0, 0), PQgetlength(res, 0, 0));
        PQclear(res);
        return 0;
    }

    PQclear(res);
    return 1;
}


int retrive_comms_from_database(char *data) {

    char cid[4];
    int row_count = 0;
    PGresult *res = NULL;

    res = PQexecPrepared(dbinf.connection, dbinf.statement_names[5], param_count[5], NULL, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(dbinf.connection));
        return 1;
    }    

    row_count = PQntuples(res);
    if (row_count > 0) {
    
        memcpy(data, PQgetvalue(res, 0, 0), PQgetlength(res, 0, 0));
        PQclear(res);
    
        // update this row from database
        memcpy(cid, data, 4);             
        const char *param_values[dbinf.param_count[6]] = {cid};        
        res = PQexecPrepared(dbinf.connection, dbinf.statement_names[6], param_count[6], param_values, NULL, NULL, 0);

        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            printf("Insert failed: %s\n", PQerrorMessage(dbinf.connection));
            PQclear(res);
   
            return 1;
        }   

        PQclear(res);

        return 0;
    
    }

    PQclear(res);
    return 1;

}


int close_database_connection() {   
    
    int status = 0;
    status = PQfinish(dbinf.connection);
    return status;

}