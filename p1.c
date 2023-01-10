#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_memory.h"
#include "partition.h"
#include <semaphore.h>
#include "bitmap.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libpq-fe.h>

#define BUFSIZE 1024*1024*2


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


idata idataobj[1160];
mdata mdataobj[9];
mupdate mupdateobj[9];


	char value[1024*1024*2];
void do_exit(PGconn *conn) {

    PQfinish(conn);
    exit(1);
}


void get_data(){

    int j,k = 0;
    char query[100];
    char id[10];
	int rows;

    PGconn *connection = PQconnectdb("user=shrikant dbname=shrikant");

    if (PQstatus(connection) == CONNECTION_BAD) {

        fprintf(stderr, "Connection to database failed: %s\n",
            PQerrorMessage(connection));
        do_exit(connection);
    }


	for(int i =0; i<9; i++){
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
			idataobj[k].msgid = atoi(PQgetvalue(result, 0, 2));
			idataobj[k].msgtype = 1;
			idataobj[k].substrid = atoi(PQgetvalue(result, j, 0));
			k++;
    	}
	
		PQclear(result);
		printf("%d\n",i); 
	}
	
    PQfinish(connection);
   
}


int get_string(int id, int substrid, char *blkptr){
	
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
		
	PGresult* res = PQprepare(connection, "insert_stmt0", "select substring from substrings where msgid = $1 and substrid = $2", 2, NULL);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Preparation of statement failed: %s\n", PQerrorMessage(connection));
        PQclear(res);
        PQfinish(connection);
        return 1;
    }

	const char* paramValues[] = {mid, msubid};

    res = PQexecPrepared(connection, "insert_stmt0", 2, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(connection));
        PQclear(res);
        PQfinish(connection);
        return 1;
    }
	memset(value, 0, strlen(value));
	strcpy(value, PQgetvalue(res, 0, 0));
	strncpy(blkptr+12, value, strlen(value));
	printf("%ld", strlen(value));
	PQclear(res);
    PQfinish(connection);
}

int do_job(char * block){


    int i, j = 0;
	int start_pos = 0;
	int empty_partition_position = -1;
    char *blkptr = NULL;

	sem_unlink(SEM_LOCK);

	sem_t *sem = sem_open(SEM_LOCK, O_CREAT, 0777, 1);
	if(sem ==  SEM_FAILED){
        printf("unable to create a semaphore");
        return -1;
    }
	
	sem_wait(sem);
	unset_all_bits(block);
	sem_post(sem);
	start_pos = -1;
	for(i = 0; i<9;){
		
		sem_wait(sem);
		empty_partition_position = get_partition(block, 0, start_pos);
	
		if(empty_partition_position >= 0){
			start_pos = empty_partition_position;
			blkptr = block + (empty_partition_position*PARTITION_SIZE);
			memcpy(blkptr, &mdataobj[i].msgtype, 4);
			memcpy(blkptr+4, &mdataobj[i].msgid, 4);
			toggle_bit(empty_partition_position, block);
			i++;
			
		}
		empty_partition_position = -1;
		sem_post(sem);
	}
	
	blkptr = NULL;
	int flag = 0;
	start_pos = -1;
	
	for(i=0; i<1160; ){
	
	sem_wait(sem);
		empty_partition_position = get_partition(block, 0, start_pos);
		
	
		if(empty_partition_position >= 0){
			start_pos = empty_partition_position;
			blkptr = block +(empty_partition_position*PARTITION_SIZE);
				
			if(j<9 && (i%10==0) && !flag){
				flag  = 1;
				
				memcpy(blkptr, &mupdateobj[j].msgtype, 4);
				memcpy(blkptr+4, &mupdateobj[j].msgid, 4);
				memcpy(blkptr+8, &mupdateobj[j].total, 4);
				j++;

			}else{
				
				memcpy(blkptr, &idataobj[i].msgtype, 4);
				memcpy(blkptr+4, &idataobj[i].msgid, 4);
				memcpy(blkptr+8, &idataobj[i].substrid, 4);
				get_string(idataobj[i].msgid, idataobj[i].substrid, blkptr);
				flag = 0;
				i++;
			}

			toggle_bit(empty_partition_position, block);
		
		}
		blkptr = NULL;
		empty_partition_position = -1;
		sem_post(sem);
		
	}
	
	sem_close(sem); // closes the named semaphore referred to by sem,
       			    // allowing any resources that the system has allocated to the
       				// calling process for this semaphore to be freed.
	
	sem_unlink(SEM_LOCK);  //removes the named semaphore referred to by name.
      					   // The semaphore name is removed immediately.  The semaphore is
                           // destroyed once all other processes that have the semaphore open
                           // close it.
}


int main(void){

    char *block = attach_memory_block(FILENAME, BLOCK_SIZE);
	if(block == NULL) {
        printf("unable to create a shared block");
        return -1;
    }
	
	get_data();
	do_job(block);
	detach_memory_block(block);
    
    return 0;
}
