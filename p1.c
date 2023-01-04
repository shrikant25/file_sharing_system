#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_memory.h"
#include <libpq-fe.h>

#define BUFSIZE 50000


typedef struct idata{
    int msgtype;
    int msgid;
    int substrid;
    char *val;
}idata;

typedef struct mdata{
    int msgtype;
    int msgid;
}mdata;

typedef struct mupdate{
    int msgtype;
    int msgid;
    int total;
}mupdate;



idata idataobj[1160];
mdata mdataobj[9];
mupdate mupdateobj[9];

void do_exit(PGconn *conn) {

    PQfinish(conn);
    exit(1);
}


void get_data(){

    int i,j,rows;
    char query[100];
    char id[10];

    PGconn *connection = PQconnectdb("user=shrikant dbname=shrikant");

    if (PQstatus(connection) == CONNECTION_BAD) {

        fprintf(stderr, "Connection to database failed: %s\n",
            PQerrorMessage(connection));
        do_exit(connection);
    }

    for(i=0; i<9; i++){

	memset(query, 0, 100);
	memset(id, 0, 10);
	
	strcpy(query, "select * from substrings where msgid = ");
	sprintf(id, "%d", i+1);
	strcat(query, id);
    	
    	PGresult *result = PQexec(connection, query);
 
 	rows = PQntuples(result);
 	
	    if (PQresultStatus(result) != PGRES_TUPLES_OK) {

		printf("No data retrieved\n");
		PQclear(result);
		do_exit(connection);
	    }
	 	
 	mupdateobj[i].msgtype = 3;
 	mupdateobj[i].msgid = atoi(PQgetvalue(result, 0, 2));
 	mupdateobj[i].total = rows;
 	
 	mdataobj[i].msgtype = 2; 
 	mdataobj[i].msgid = atoi(PQgetvalue(result, 0, 2));
 	
 	
 	for(j=0; j<rows; j++){
 		idataobj[i*9+j].msgid = atoi(PQgetvalue(result, 0, 2));
 		idataobj[i*9+j].msgtype = 1;
 		idataobj[i*9+j].substrid = atoi(PQgetvalue(result, j, 0));
 	}
	
	PQclear(result);
     
    }

    PQfinish(connection);
   
}


int main(void){

 
    int i, j;
    char *block = attach_memory_block(FILENAME, BLOCK_SIZE);
    if(block == NULL) {
        printf("unable to create block");
        return -1;
    }


//    strncpy(block,, BLOCK_SIZE);
    get_data();
       for(i=0; i<9; i++){
       		printf(" insert in substrings table : msgid : %d, substrid : %d, msgtype : %d\n", idataobj[i*9].msgid, idataobj[i*9].substrid, idataobj[i*9].msgtype);
       		printf(" update in parts table      : msgid : %d, total    : %d, msgtype : %d\n", mupdateobj[i].msgid, mupdateobj[i].total, mupdateobj[i].msgtype);
       		printf(" insert in parts table      : msgid : %d, msgtype  : %d\n", mdataobj[i].msgid, mdataobj[i].msgtype);
       		printf("\n");
       }
    
    
    
    detach_memory_block(block);
    return 0;
}
