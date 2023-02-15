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


int retrive_data_from_database(char *) {

}


int retrive_comms_from_database(char *) {

}


int retrive_commr_from_database(char *) {

}


int store_comms_into_database(char *) {

}


int store_commr_into_database(char *) {

}

int close_database_connection() {   
    
    int status = 0;
    status = PQfinish(dbinf.connection);
    return status;

}