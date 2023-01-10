#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <semaphore.h>
#include "shared_memory.h"
#include "bitmap.h"
#include "partition.h"
#include <unistd.h>
#include <libpq-fe.h>
#include <fcntl.h>
#include <sys/stat.h>


#define BUFSIZE 2*1024*1024


typedef struct idata{
    int msgid;
    int substrid;
    char val[BUFSIZE];
}idata;

typedef struct mdata{
    int msgid;
}mdata;

typedef struct mupdate{
    int msgid;
    int total;
}mupdate;

char total[10];
char cur[10];
char msgid[10];
char substrid[10];

void do_exit(PGconn *conn) {

    PQfinish(conn);
    exit(1);
}


int get_sinsert_query(char *blkptr, PGconn *conn){
 PGresult* res;
    idata d;

    memcpy(&d.msgid, blkptr+4, 4);
    memcpy(&d.substrid, blkptr+8, 4);
    strncpy(d.val, blkptr+12, (1024*1024));
    
    sprintf(msgid, "%d", d.msgid);
    sprintf(substrid, "%d", d.substrid);

    const char* paramValues[] = {substrid, d.val, msgid};
    
    // Execute the INSERT statement
    res = PQexecPrepared(conn, "insert_stmt", 3, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        return 1;
    }

    // Clear the result set and close the connection
    PQclear(res);
    return 0;
}


int get_minsert_query(char *blkptr, PGconn *conn){
     PGresult* res;
    mdata m;

    memset(msgid, 0 , 10);
    memset(cur, 0, 10);
    memset(total, 0, 10);

    memcpy(&m.msgid, blkptr+4, 4);    
    sprintf(msgid, "%d", m.msgid);
    sprintf(cur, "%d", 0);
    sprintf(total, "%d", 0);

    const char* paramValues[] = {msgid, cur, total};
    

    // Execute the INSERT statement
    res = PQexecPrepared(conn, "insert_stmt2", 3, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        return 1;
    }

    // Clear the result set and close the connection
    PQclear(res);
    return 0;    
}


int get_mupdate_query(char *blkptr, PGconn *conn){
 PGresult* res;
    mupdate u;

    memcpy(&u.msgid, blkptr+4, 4);
    memcpy(&u.total, blkptr+8, 4);

    memset(msgid, 0, 10);
    memset(total, 0, 10);

    sprintf(msgid, "%d", u.msgid);
    sprintf(total, "%d", u.total);

    const char* paramValues[] = {msgid, total};
    

    res = PQexecPrepared(conn, "insert_stmt3", 2, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        return 1;
    }

    PQclear(res);
    return 0; 

}


int main(void){
    
    int querytype;
    int filled_partition_position = -1;
    char *blkptr = NULL;
    char *block = attach_memory_block(FILENAME, BLOCK_SIZE);
    int start_pos = 0;
   
    if(block == NULL) {
        printf("failed to get block");
        return -1;
    }

    PGconn *connection = PQconnectdb("user=shrikant dbname=shrikant");
    if (PQstatus(connection) == CONNECTION_BAD) {

        fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(connection));
        do_exit(connection);
    }

   sem_t *sem = sem_open(SEM_LOCK, 0);
   if(sem ==  SEM_FAILED){
        printf("unable to create a semaphore");
        return -1;
    }
    
    start_pos = -1;

    PGresult* res1 = PQprepare(connection, "insert_stmt", "INSERT INTO psubstrings VALUES ($1, $2, $3)", 3, NULL);

    if (PQresultStatus(res1) != PGRES_COMMAND_OK) {
        printf("Preparation of statement failed: %s\n", PQerrorMessage(connection));
        PQclear(res1);
        PQfinish(connection);
        return 1;
    }

    PGresult* res2 = PQprepare(connection, "insert_stmt2", "INSERT INTO psubparts VALUES ($1, $2, $3)", 3, NULL);

    if (PQresultStatus(res2) != PGRES_COMMAND_OK) {
        printf("Preparation of statement failed: %s\n", PQerrorMessage(connection));
        PQclear(res2);
        PQfinish(connection);
        return 1;
    }


    PGresult* res3 = PQprepare(connection, "insert_stmt3", "update psubparts set total = $2 where msgid = $1", 2, NULL);

    if (PQresultStatus(res3) != PGRES_COMMAND_OK) {
        printf("Preparation of statement failed: %s\n", PQerrorMessage(connection));
        PQclear(res3);
        PQfinish(connection);
        return 1;
    }


    while(1){
     
       sem_wait(sem);         
        
        filled_partition_position = get_partition(block, 1, start_pos);
        
        
      
		if(filled_partition_position >= 0){
           start_pos = filled_partition_position;

            blkptr = block +(filled_partition_position*PARTITION_SIZE);
            
            memcpy(&querytype, blkptr, 4);
          
            if(querytype == 1)
                get_sinsert_query(blkptr, connection);
            else if(querytype == 2)
                get_minsert_query(blkptr, connection);
            else 
                get_mupdate_query(blkptr, connection);
        
             
                memset(blkptr, 0, PARTITION_SIZE);
            blkptr = NULL;
            toggle_bit(filled_partition_position, block);
            
		
        }
        filled_partition_position = -1;
       
        sem_post(sem);
    }

    sem_close(sem);

    PQfinish(connection);
    detach_memory_block(block);
        
    return 0;
}
