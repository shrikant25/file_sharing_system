#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <semaphore.h>
#include "shared_memory.h"
#include "bitmap.h"
#include "partition.h"
#include <libpq-fe.h>
#include <fcntl.h>
#include <sys/stat.h>
#define BUFSIZE 50000


typedef struct idata{
    int msgid;
    int substrid;
    char val[1024*1024];
}idata;

typedef struct mdata{
    int msgid;
}mdata;

typedef struct mupdate{
    int msgid;
    int total;
}mupdate;

char total[100];
char query[100];
char msgid[10];
char substrid[10];

void do_exit(PGconn *conn) {

    PQfinish(conn);
    exit(1);
}


void get_sinsert_query(char *blkptr){

    idata d;

    memcpy(&d.msgid, blkptr+4, 4);
    memcpy(&d.substrid, blkptr+8, 4);
    memcpy(&d.val, blkptr+12, (1024*1024));
    memset(query, 0, 100);
    
    sprintf(msgid, "%d", d.msgid);
    sprintf(substrid, "%d", d.substrid);

    strcpy(query, "insert into psubstrings values( ");
    strcat(query, substrid);
    strcat(query, " , ' ");
    strcat(query, d.val); 
    strcat(query, " ' , ");
    strcat(query, msgid);
    strcat(query, " ) ");
    
}


void get_minsert_query(char *blkptr){
    
    mdata m;
    memcpy(&m.msgid, blkptr+4, 4);

    memset(query, 0, 100);
    memset(msgid, 0 , 10);

    sprintf(msgid, "%d", m.msgid);
    strcpy(query, "insert into psubparts values( ");
    strcat(query, msgid);
    strcat(query, " , ");
    strcat(query, " 0 , ");
    strcat(query, " 0 )");    

}


void get_mupdate_query(char *blkptr){

    mupdate u;

    memcpy(&u.msgid, blkptr+4, 4);
    memcpy(&u.total, blkptr+8, 4);

    memset(query, 0, 100);
    memset(msgid, 0, 10);
    memset(total, 0, 10);

    sprintf(msgid, "%d", u.msgid);
    sprintf(total, "%d", u.total);

    strcpy(query, "update psubparts set total = ");
    strcat(query, total);
    strcat(query, " where msgid = ");
    strcat(query, msgid);

}


int main(void){
    
    int querytype;
    int filled_partition_position = -1;
    char *blkptr = NULL;
    char *block = attach_memory_block(FILENAME, BLOCK_SIZE);
  
    if(block == NULL) {
        printf("failed to get block");
        return -1;
    }

    PGconn *connection = PQconnectdb("user=shrikant dbname=shrikant");
    if (PQstatus(connection) == CONNECTION_BAD) {

        fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(connection));
        do_exit(connection);
    }

	sem_t *semptr = sem_open(SEM_SHM_LOCK, O_CREAT, 0777, 0);
	if(semptr == SEM_FAILED){
		printf("unable to create a p1 lock");
		return -1;
	}

    while(1){
        
        sem_wait(semptr);
        filled_partition_position = get_partition(block, 1);
		if(filled_partition_position >= 0){
         
            blkptr = block +(filled_partition_position*PARTITION_SIZE);
            
            memcpy(&querytype, blkptr, 4);
            printf("%d\n", querytype);
            if(querytype == 1)
                get_sinsert_query(blkptr);
            else if(querytype == 2)
                get_minsert_query(blkptr);
            else 
                get_mupdate_query(blkptr);

            PQexec(connection, query);
   
            blkptr = NULL;
            toggle_bit(filled_partition_position, block);
            filled_partition_position = -1;
		
        }
        else
            printf("No data in shared space");
        sem_post(semptr);
    }

    sem_close(semptr);
    PQfinish(connection);
    detach_memory_block(block);
        
    return 0;
}
