#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <libpq-fe.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <semaphore.h>
#include "shared_memory.h"
#include "partition.h"
#include "processor_r.h"

int process_status = 1;

datablocks dblks;
semlocks smlks;
PGconn *connection;

#define statement_count 3

db_statements dbs[statement_count] = {
    { 
      .statement_name = "r_insert_data",  
      .statement = "INSERT INTO job_scheduler(jobdata, jstate, jtype, \
                    jsource, jobid, jparent_jobid, jdestination, \
                    jpriority) VALUES($2, 'N-0', '0', $1, GEN_RANDOM_UUID(), \
                    (select jobid from job_scheduler where jparent_jobid = jobid), 0, 0);",
      .param_count = 2,
    },
    { 
      .statement_name = "r_insert_conns", 
      .statement = "INSERT INTO receiving_conns (rfd, ripaddr, rcstatus) \
                    VALUES ($1, $2, $3) ON CONFLICT (rfd) DO UPDATE SET rcstatus = ($3);",
      .param_count = 3,
    },
    {
      .statement_name = "pr_storelogs", 
      .statement = "INSERT INTO logs (log) VALUES ($1);",
      .param_count = 1,
    }
};


int connect_to_database() 
{   
    connection = PQconnectdb("user = shrikant dbname = shrikant");
    if (PQstatus(connection) == CONNECTION_BAD) {
        syslog(LOG_NOTICE, "Connection to database failed: %s\n", PQerrorMessage(connection));
        return -1;
    }

    return 0;
}


int prepare_statements() 
{    
    int i;

    for(i = 0; i<statement_count; i++){

        PGresult* res = PQprepare(connection, dbs[i].statement_name, 
                                    dbs[i].statement, dbs[i].param_count, NULL);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            syslog(LOG_NOTICE, "Preparation of statement failed: %s\n", PQerrorMessage(connection));
            return -1;
        }

        PQclear(res);
    }

    return 0;
}


int retrive_commr_from_database(char *data) 
{
    // int row_count = 0;
    // PGresult *res = NULL;
    // error[100];
    // res = PQexecPrepared(connection, dbs[2].statement_name, dbs[2].param_count, NULL, NULL, NULL, 0);

    // if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    //     memset(error, 0, sizeof(error));
    //        sprintf(error, "%s %s", "data storing failed failed", PQerrorMessage(connection));
    //        store_log(error);
    //        return -1;
    // }    

    // row_count = PQntuples(res);
    // if (row_count > 0) {
    //     memcpy(data, PQgetvalue(res, 0, 0), PQgetlength(res, 0, 0));
    //     PQclear(res);
    //     return 0;
    // }

    // PQclear(res);
    return -1;
}


void store_log(char *logtext) {

    PGresult *res = NULL;
    char log[100];
    strncpy(log, logtext, strlen(logtext));

    const char *const param_values[] = {log};
    const int paramLengths[] = {sizeof(log)};
    const int paramFormats[] = {0};
    int resultFormat = 0;
    
    res = PQexecPrepared(connection, "r_storelog", 2, param_values, paramLengths, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        syslog(LOG_NOTICE, "logging failed %s , log %s\n", PQerrorMessage(connection), log);
    }

    PQclear(res);
}


int store_data_in_database(newmsg_data *nmsg) 
{
    PGresult *res = NULL;
    
    char fd[11];
    char error[100];

    sprintf(fd, "%d", nmsg->data1);

    const int paramLengths[] = {sizeof(nmsg->data1), sizeof(nmsg->data)};
    const int paramFormats[] = {0, 1};
    int resultFormat = 0;

    const char *const param_values[] = {fd, nmsg->data};
    
    res = PQexecPrepared(connection, dbs[0].statement_name, 
                                    dbs[0].param_count, param_values, paramLengths, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        memset(error, 0, sizeof(error));
        sprintf(error, "%s %s", "data storing failed", PQerrorMessage(connection));
        store_log(error);
        return -1;
    }

    PQclear(res);

    return 0;
}


int store_commr_into_database(receivers_message *rcvm) 
{
    PGresult *res = NULL;

    char fd[11];
    char ipaddr[11];
    char status [11];
    char error[100];

    sprintf(fd, "%d", rcvm->fd);
    sprintf(ipaddr, "%d", rcvm->ipaddr);
    sprintf(status, "%d", rcvm->status);

    const int paramLengths[] = {sizeof(fd), sizeof(ipaddr), sizeof(status)};
    const int paramFormats[] = {0, 0, 0};
    int resultFormat = 0;
   
    const char *const param_values[] = {fd, ipaddr, status};
    
    res = PQexecPrepared(connection, dbs[1].statement_name, dbs[1].param_count, param_values, paramLengths, paramFormats, resultFormat);
   
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        memset(error, 0, sizeof(error));
        sprintf(error, "%s %s", "comms storing failed failed", PQerrorMessage(connection));
        store_log(error);
        return -1;
    }

    PQclear(res);

    return 0;
}


int get_data_from_receiver() 
{
    int subblock_position = -1;
    char *blkptr = NULL;
    newmsg_data rcond;

    sem_wait(smlks.sem_lock_datar);         
    subblock_position = get_subblock(dblks.datar_block, 1, 3);
    
    if (subblock_position >= 0) {

        blkptr = dblks.datar_block + (TOTAL_PARTITIONS/8) + subblock_position * DPARTITION_SIZE;
        
        memset(&rcond, 0, sizeof(rcond));
        memcpy(&rcond, blkptr, sizeof(rcond));

        store_data_in_database(&rcond);

        blkptr = NULL;
        toggle_bit(subblock_position, dblks.datar_block, 3);
    
    }

    sem_post(smlks.sem_lock_datar);
}


int communicate_with_receiver() 
{
    int subblock_position = -1;
    char *blkptr = NULL;
    char data[CPARTITION_SIZE];
    receivers_message rcvm;

    sem_wait(smlks.sem_lock_commr);         
    subblock_position = get_subblock(dblks.commr_block, 0, 1);
    
    if(subblock_position >= 0) {

        blkptr = dblks.commr_block  + (TOTAL_PARTITIONS/8) + subblock_position * CPARTITION_SIZE;
        
        memset(data, 0, CPARTITION_SIZE);
        if (retrive_commr_from_database(data) != -1) {

            memcpy(blkptr, data, CPARTITION_SIZE);
            memset(data, 0, CPARTITION_SIZE);  
            toggle_bit(subblock_position, dblks.commr_block, 1);

        }
        blkptr = NULL;
    }

 
    subblock_position = -1;
    subblock_position = get_subblock(dblks.commr_block, 1, 2);
    
    if (subblock_position >= 0) {

        blkptr = dblks.commr_block + (TOTAL_PARTITIONS/8) + subblock_position*CPARTITION_SIZE;
        
        memset(&rcvm, 0, sizeof(rcvm));
        memcpy(&rcvm, blkptr, sizeof(rcvm));
        store_commr_into_database(&rcvm);
          
        blkptr = NULL;
        toggle_bit(subblock_position, dblks.commr_block, 2);
    
    }

    sem_post(smlks.sem_lock_commr);   
}



int run_process() 
{
    struct timespec ts;
    ts.tv_sec = 1;
    ts.tv_nsec = 0;

    while (process_status) {
        communicate_with_receiver();
        get_data_from_receiver();
        if (sem_timedwait(smlks.sem_lock_sigdr, &ts) == -1 && strerror(errno) == ETIMEDOUT) {
            // check for noti from psql
        }
        else{
            // call function to read messages
            //
        }
        /*
            
            if signal from data base read messae from database
            
        
        */
       /*
            if signal from receivers read from reciever and store in database
       */
    }  
}


int main(void) 
{
    int status = 0;
    if (connect_to_database() == -1) { return -1; }
    if (prepare_statements() == -1) { return -1; }   

    sem_unlink(SEM_LOCK_DATAR);
    sem_unlink(SEM_LOCK_COMMR);

    smlks.sem_lock_datar = sem_open(SEM_LOCK_DATAR, O_CREAT, 0777, 1);
    smlks.sem_lock_commr = sem_open(SEM_LOCK_COMMR, O_CREAT, 0777, 1);
    smlks.sem_lock_sigdr = sme_open(SEM_LOCK_SIG_D, O_CREAT, 0777, 1);
    
    
    if (smlks.sem_lock_datar == SEM_FAILED || smlks.sem_lock_commr == SEM_FAILED) {
        store_log("failed to intialize locks");
        status = -1;
    }

    /*
    
    sid.sem_sig_id = semget(get_key(SEM_ID_SIG_R, PROJECT_ID_SIG_R), 1, 0666);
    if (sem_id < 0) {
        store_log("failed create to semaphore for signaling");
        return -1;
    }
    
    */



    dblks.datar_block = attach_memory_block(FILENAME_R, DATA_BLOCK_SIZE, (unsigned char)PROJECT_ID_DATAR);
    dblks.commr_block = attach_memory_block(FILENAME_R, COMM_BLOCK_SIZE, (unsigned char)PROJECT_ID_COMMR);
    if (!(dblks.datar_block && dblks.commr_block)) {
        store_log("failed to get shared memory");
        return -1; 
    }

    unset_all_bits(dblks.commr_block, 2);
    unset_all_bits(dblks.commr_block, 3);
    unset_all_bits(dblks.datar_block, 1);
    
    run_process(); 
    
    PQfinish(connection);    

    sem_close(smlks.sem_lock_datar);
    sem_close(smlks.sem_lock_commr);
    
    detach_memory_block(dblks.datar_block);
    detach_memory_block(dblks.commr_block);
    
    destroy_memory_block(dblks.datar_block, PROJECT_ID_DATAR);
    destroy_memory_block(dblks.commr_block, PROJECT_ID_COMMR);
    
    return 0;
}   