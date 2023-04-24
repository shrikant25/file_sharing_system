#include <semaphore.h>
#include <signal.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <libpq-fe.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/shm.h>
#include "shared_memory.h"
#include "partition.h"
#include "processor_r.h"

int process_status = 1;

PGconn *connection;
datablocks dblks;
semlocks smlks;

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
    newmsg_data nmsg;

    sem_wait(smlks.sem_lock_datar);         
    subblock_position = get_subblock(dblks.datar_block, 1, 3);
    
    if (subblock_position >= 0) {

        blkptr = dblks.datar_block + (TOTAL_PARTITIONS/8) + subblock_position * DPARTITION_SIZE;
        
        memset(&nmsg, 0, sizeof(nmsg));
        memcpy(&nmsg, blkptr, sizeof(nmsg));

        store_data_in_database(&nmsg);

        blkptr = NULL;
        toggle_bit(subblock_position, dblks.datar_block, 3);
    
    }

    sem_post(smlks.sem_lock_datar);
    return subblock_position;
}


int get_message_from_receiver() {
    
    int subblock_position = -1;
    char *blkptr = NULL;
    receivers_message rcvm;

    sem_wait(smlks.sem_lock_commr);         
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
    return subblock_position;
}

/*
int send_message_to_receiver() {
    
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
    
    sem_post(smlks.sem_lock_commr);    
    return subblock_position;
}
*/

int run_process() 
{
    int status = 0;
    char data[CPARTITION_SIZE];

    while (process_status) {

        sem_wait(smlks.sem_lock_sigr); 
        get_message_from_receiver();
        get_data_from_receiver();           
  /*      if (retrive_commr_from_database(data) != -1) {
            send_message_to_receiver(data);
        }*/    
    }  
}

int connect_to_database() 
{   
    connection = PQconnectdb("user = shrikant dbname = shrikant");

    if (PQstatus(connection) != CONNECTION_OK) {
        syslog(LOG_NOTICE, "Connection to database failed: %s\n", PQerrorMessage(connection));
        return -1;
    }

    return 0;
}


int prepare_statements() 
{    
    int i, status = 0;

    for (i = 0; i<statement_count; i++){

        PGresult* res = PQprepare(connection, dbs[i].statement_name, dbs[i].statement, dbs[i].param_count, NULL);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            syslog(LOG_NOTICE, "Preparation of statement failed: %s\n", PQerrorMessage(connection));
            status = -1;
            PQclear(res);
            break;
        }

        PQclear(res);
    }
    
    return status;
}


int main(void) 
{
    int status = 0;

    if (connect_to_database() == -1) { return -1; }
    if (prepare_statements() == -1) { return -1; }   
    
    sem_unlink(SEM_LOCK_DATAR);
    sem_unlink(SEM_LOCK_COMMR);
    sem_unlink(SEM_LOCK_SIG_S);

    smlks.sem_lock_datar = sem_open(SEM_LOCK_DATAR, O_CREAT, 0777, 1);
    smlks.sem_lock_commr = sem_open(SEM_LOCK_COMMR, O_CREAT, 0777, 1);
    smlks.sem_lock_sigr = sem_open(SEM_LOCK_SIG_R, O_CREAT, 0777, 0);
    
    if (smlks.sem_lock_sigr == SEM_FAILED || smlks.sem_lock_datar == SEM_FAILED || smlks.sem_lock_commr == SEM_FAILED) {
        store_log("failed to intialize locks");
        status = -1;
    }

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
    sem_close(smlks.sem_lock_sigr);
    
    detach_memory_block(dblks.datar_block);
    detach_memory_block(dblks.commr_block);
    
    destroy_memory_block(dblks.datar_block, PROJECT_ID_DATAR);
    destroy_memory_block(dblks.commr_block, PROJECT_ID_COMMR);
    
    return 0;
}   