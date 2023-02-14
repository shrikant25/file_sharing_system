#include <libpq-fe.h>
#include "partition.h"
#include "processor_db.h"

database_info dbinf;
//if any error occurs try to mitigate it here only
//if mitigation fails then return -1
int connect_to_database() {
   
    dbinf.connection = PQconnectdb("user=shrikant dbname=shrikant");

    if (PQstatus(dbinf.connection) == CONNECTION_BAD) {
        fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(connection));
        // mitigate
        //if fails then return -1
    }

    return 0;
}


int prepare_statments() {


    PGresult* res = PQprepare(dbinf.connection, dbinf.prpd_stmts.ps_strdata_db, 
                            "INSERT INTO raw_data (data) values($1)", 1, NULL);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Preparation of statement failed: %s\n", PQerrorMessage(dbinf.connection));
    }

    PQclear(res);


    PGresult* res = PQprepare(dbinf.connection, dbinf.prpd_stmts.ps_rtrdata_db, 
                            "INSERT INTO receiver_status (data) values($1)", 1, NULL);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Preparation of statement failed: %s\n", PQerrorMessage(dbinf.connection));
    }

    PQclear(res);


    PGresult* res = PQprepare(dbinf.connection, dbinf.prpd_stmts.ps_strcomm_db, 
                            "INSERT INTO raw_data (data) values($1)", 1, NULL);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Preparation of statement failed: %s\n", PQerrorMessage(dbinf.connection));
    }

    PQclear(res);


    PGresult* res = PQprepare(dbinf.connection, dbinf.prpd_stmts.ps_rtrcomm_db, 
                            "INSERT INTO raw_data (data) values($1)", 1, NULL);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Preparation of statement failed: %s\n", PQerrorMessage(dbinf.connection));
    }
    return 0;

}


int store_data_in_database(char *data) {

    const char *paramvalues[] = {data}
    
    
    res = PQexecPrepared(dbinf.connection, "dbinf.prpd_stmts.ps_strdata_db", 1, paramvalues, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(dbinf.connection));
    }

    PQclear(res);
    return 0;
}


int store_comm_from_database(char *data) {

    const char *paramvalues[] = {data}
    
    res = PQexecPrepared(dbinf.connection, "dbinf.prpd_stmts.ps_strdata_db", 1, paramvalues, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(dbinf.connection));
    }

    PQclear(res);
    return 0;

}

// todo
int retrive_data_from_database(char *data) {

    const char *paramvalues[] = {data}
    
    

    res = PQexecPrepared(dbinf.connection, "dbinf.prpd_stmts.ps_rtrdata_db", 1, paramvalues, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(dbinf.connection));
    }

    PQclear(res);
    return 0;

}

// todo
int retrive_comm_from_database(char *data) {
    
    const char *paramvalues[] = {data}
    
    res = PQexecPrepared(dbinf.connection, "dbinf.prpd_stmts.ps_rtrcomm_db", 1, paramvalues, NULL, NULL, 0);
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