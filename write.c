#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_memory.h"
#include <libpq-fe.h>

#define BUFSIZE 50000

void do_exit(PGconn *conn) {

    PQfinish(conn);
    exit(1);
}

void get_data(char *block){


    PGconn *connection = PQconnectdb("user=shrikant dbname=shrikant");

    if (PQstatus(connection) == CONNECTION_BAD) {

        fprintf(stderr, "Connection to database failed: %s\n",
            PQerrorMessage(connection));
        do_exit(connection);
    }


   
    PGresult *result = PQexec(connection, "select substring from substrings where msgid = 1 and substrid = 1");


    if (PQresultStatus(result) != PGRES_TUPLES_OK) {

        printf("No data retrieved\n");
        PQclear(result);
        do_exit(connection);
    }
    
    strncpy(block, PQgetvalue(result, 0, 0), BLOCK_SIZE);
    PQclear(result);

    PQfinish(connection);
   
  
}


int main(void){

 
    char *block = attach_memory_block(FILENAME, BLOCK_SIZE);
    if(block == NULL) {
        printf("unable to create block");
        return -1;
    }

 
    get_data(block);
    
    detach_memory_block(block);
    return 0;
}