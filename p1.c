#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_memory.h"
#include "partition.h"
#include "bitmap.h"
#include <libpq-fe.h>

#define BUFSIZE 50000

typedef struct idata{
    int msgtype;
    int msgid;
    int substrid;
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

int rows;
idata idataobj[1160];
mdata mdataobj[9];
mupdate mupdateobj[9];

void do_exit(PGconn *conn) {

    PQfinish(conn);
    exit(1);
}


void get_data(){

    int i,j;
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


void randomize(){

	int i;
	idata temp;
	
	for(i = 0; i<rows/2; i++){
		memcpy(temp, idataobj[i], sizeof(idata));
		memcpy(idataobj[i], idataobj[i+3], sizeof(idata));
		memcpy(idataobj[i+3], temp, sizeof(idata));
	}
	
	mupdate temp2;
	memcpy(temp2, mupdateobj[5], sizeof(mupdate));
	memcpy(mupdateobj[5], mupdateobj[2],sizeof(mupdate));
	memcpy(mupdateobj[2], temp2, sizeof(mupdate));
	
}


void get_string(int id, int substrid){
	
	int i,j;
    char query[100];
    char mid[10];
    char msubid[10];

    PGconn *connection = PQconnectdb("user=shrikant dbname=shrikant");

    if (PQstatus(connection) == CONNECTION_BAD) {

        fprintf(stderr, "Connection to database failed: %s\n",
            PQerrorMessage(connection));
        do_exit(connection);
    }

	sprintf(mid, "%d", id);
	sprintf(msubid, "%d", substrid);
		
	strcpy(query, "select substring from substrings where msgid = ");
	strcat(query, mid);
	strcat(query, "and substrid = ");
	strcat(query, msubid);
	
	 
    PGresult *result = PQexec(connection, query);
	
	rows = PQntuples(result);

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {

		printf("No data retrieved\n");
		PQclear(result);
		do_exit(connection);
    }

	strcpy(block+12, PQgetvalue(result, 0, 0));
	
	PQclear(result);
    PQfinish(connection);
}

//int work(int sockfd){
int work(){

 
    int i, j = 0;
	int empty_partition_position = -1;
	//char buffer[256];
    char *block = attach_memory_block(FILENAME, BLOCK_SIZE);
    char *blkptr = NULL;
	
	if(block == NULL) {
        printf("unable to create a shared block");
        return -1;
    }

    unset_all_bits(block);

	get_data();

	//memset(buffer, 0, 256);
	//strcpy(buffer, "1");
	//n = write(sockfd,buffer,strlen(buffer));
	//if (n < 0) error("ERROR writing to socket");

	for(i = 0; i<9;){
		
		empty_partition_position = get_partition(block, 0);
		if(empty_partition_position >= 0){
			
			blkptr = block +(empty_partition_position*PARTITION_SIZE);
			memcpy(blkptr, mdataobj[i].msgtype, 4);
			memcpy(blkptr+4, mdataobj[i].msgid, 4);
			toggle_bit(empty_partition_position, block);
			i++;
		}
		else
			printf("no partition available");

	}
	
	for(i=0; i<rows; ){
		
		empty_partition_position = get_partition();
		if(empty_partition_position >= 0){
						
			blkptr = block +(empty_partition_position*PARTITION_SIZE);
				
			if(j<9 && !(i%30)){
				
				memcpy(blkptr, mupdate[j].msgtype, 4);
				memcpy(blkptr+4, mupdate[j].msgid, 4);
				memcpy(blkptr+8, mupdate[j].msgtotal, 4);
				j++;

			}else{
				
				memcpy(blkptr, idataobj[i].msgtype, 4);
				memcpy(blkptr+4, idataobj[i].msgid, 4);
				memcpy(blkptr+8, idataobj[i].substrid, 4);
				get_string(idataobj[i].msgid, idataobj[i].substrid);
				i++;
			}

			toggle_bit(empty_partition_position, block);
		
		}
		else
			printf("no partition available");

	}
    
    detach_memory_block(block);
    
    return 0;
}
