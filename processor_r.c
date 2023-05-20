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

PGconn *connection;
semlocks sem_lock_datar;
semlocks sem_lock_commr;
semlocks sem_lock_sigr;
datablocks datar_block;
datablocks commr_block;


void store_log (char *logtext) 
{

    PGresult *res = NULL;
    char log[100];
    memset(log, 0, sizeof(log));
    strncpy(log, logtext, strlen(logtext));

    const char *const param_values[] = {log};
    const int paramLengths[] = {sizeof(log)};
    const int paramFormats[] = {0};
    int resultFormat = 0;
    
    res = PQexecPrepared(connection,  dbs[2].statement_name,  dbs[2].param_count, param_values, paramLengths, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        syslog(LOG_NOTICE, "logging failed %s , log %s\n", PQerrorMessage(connection), log);
    }

    PQclear(res);
}


int store_data_in_database (newmsg_data *nmsg) 
{
    PGresult *res = NULL;
    
    char fd[11];
    char error[100];

    sprintf(fd, "%d", nmsg->data1);

    const int paramLengths[] = {sizeof(nmsg->data1), nmsg->data2};
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


int store_commr_into_database (receivers_message *rcvm) 
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


int get_data_from_receiver () 
{
    int subblock_position = -1;
    char *blkptr = NULL;
    newmsg_data nmsg;

    sem_wait(sem_lock_datar.var);         
    subblock_position = get_subblock(datar_block.var, 1, 3);
    
    if (subblock_position >= 0) {

        blkptr = datar_block.var + (TOTAL_PARTITIONS/8) + subblock_position * DPARTITION_SIZE;
        
        memset(&nmsg, 0, sizeof(nmsg));
        memcpy(&nmsg, blkptr, sizeof(nmsg));

        store_data_in_database(&nmsg);

        blkptr = NULL;
        toggle_bit(subblock_position, datar_block.var, 3);
    
    }

    sem_post(sem_lock_datar.var);
    return subblock_position;
}


int get_message_from_receiver () 
{
    int subblock_position = -1;
    char *blkptr = NULL;
    receivers_message rcvm;

    sem_wait(sem_lock_commr.var);         
    subblock_position = get_subblock(commr_block.var, 1, 2);
    
    if (subblock_position >= 0) {

        blkptr = commr_block.var + (TOTAL_PARTITIONS/8) + subblock_position*CPARTITION_SIZE;
        
        memset(&rcvm, 0, sizeof(rcvm));
        memcpy(&rcvm, blkptr, sizeof(rcvm));
        store_commr_into_database(&rcvm);
        toggle_bit(subblock_position, commr_block.var, 2);
    
    }

    sem_post(sem_lock_commr.var);
    return subblock_position;
}


int get_comms_from_database (char *blkptr) 
{
    PGresult *res = NULL;
    capacity_info cpif;

    char error[100];
    
    res = PQexecPrepared(connection, dbs[3].statement_name, dbs[3].param_count, NULL, NULL, NULL, 0);
   
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) <= 0) {
        memset(error, 0, sizeof(error));
        sprintf(error, "retriving comms for receiver failed %s", PQerrorMessage(connection));
        store_log(error);
        PQclear(res);
        return -1;
    }    
    
    memset(&cpif, 0, sizeof(capacity_info));
    strncpy(cpif.ipaddress, PQgetvalue(res, 0, 0), PQgetlength(res, 0, 0));
    cpif.capacity = atoi(PQgetvalue(res, 0, 1));
    memset(error, 0, sizeof(error));
    sprintf(error, "ip %s, cp %d", cpif.ipaddress, cpif.capacity);
    store_log(error);
    memcpy(blkptr, &cpif, sizeof(capacity_info));

    PQclear(res);
    return 0;
}


int send_msg_to_receiver () 
{
    int subblock_position;
    char *blkptr = NULL;
    
    sem_wait(sem_lock_commr.var);
    subblock_position = get_subblock(commr_block.var, 0, 1);

    if (subblock_position >= 0) {

        blkptr = commr_block.var + (TOTAL_PARTITIONS/8) + subblock_position*CPARTITION_SIZE;
        
        if (get_comms_from_database(blkptr) != -1) {
            toggle_bit(subblock_position, commr_block.var, 1);
        }
    }

    sem_post(sem_lock_commr.var);
    return subblock_position;
} 


int run_process () 
{
    int status = 0;
    char data[CPARTITION_SIZE];

    while (1) {

        sem_wait(sem_lock_sigr.var); 
        get_message_from_receiver();
        get_data_from_receiver();           
        send_msg_to_receiver(); 
    }  
}


int connect_to_database (char *conninfo) 
{   
    connection = PQconnectdb(conninfo);

    if (PQstatus(connection) != CONNECTION_OK) {
        syslog(LOG_NOTICE, "Connection to database failed: %s\n", PQerrorMessage(connection));
        return -1;
    }

    return 0;
}


int prepare_statements () 
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


int main (int argc, char *argv[]) 
{
    int status = -1;
    int conffd = -1;
    char buf[500];
    char db_conn_command[100];
    char username[30];
    char dbname[30];

    if (argc != 2) {
        syslog(LOG_NOTICE,"invalid arguments");
        return -1;
    }
   
    if ((conffd = open(argv[1], O_RDONLY)) == -1) {
        syslog(LOG_NOTICE, "failed to open configuration file");
        return -1;
    }

    if (read(conffd, buf, sizeof(buf)) > 0) {
       
        sscanf(buf, "SEM_LOCK_DATAR=%s\nSEM_LOCK_COMMR=%s\nSEM_LOCK_SIG_R=%s\nPROJECT_ID_DATAR=%d\nPROJECT_ID_COMMR=%d\nUSERNAME=%s\nDBNAME=%s", sem_lock_datar.key, sem_lock_commr.key, sem_lock_sigr.key, &datar_block.key, &commr_block.key, username, dbname);
    }
    else {
        syslog(LOG_NOTICE, "failed to read configuration file");
        return -1;
    }
    
    close(conffd);

    sprintf(db_conn_command, "user=%s dbname=%s", username, dbname);
    if (connect_to_database(db_conn_command) == -1) { return -1; }
    if (prepare_statements() == -1) { return -1; }   
    
    sem_lock_datar.var = sem_open(sem_lock_datar.key, O_CREAT, 0777, 1);
    sem_lock_commr.var = sem_open(sem_lock_commr.key, O_CREAT, 0777, 1);
    sem_lock_sigr.var = sem_open(sem_lock_sigr.key, O_CREAT, 0777, 0);
    
    if (sem_lock_sigr.var == SEM_FAILED || sem_lock_datar.var == SEM_FAILED || sem_lock_commr.var == SEM_FAILED) {
        store_log("failed to intialize locks");
        return -1;
    }

    datar_block.var = attach_memory_block(FILENAME_R, DATA_BLOCK_SIZE, datar_block.key);
    commr_block.var = attach_memory_block(FILENAME_R, COMM_BLOCK_SIZE, commr_block.key);
    if (!(datar_block.var && commr_block.var)) {
        store_log("failed to get shared memory");
        return -1; 
    }

    unset_all_bits(commr_block.var, 2);
    unset_all_bits(commr_block.var, 3);
    unset_all_bits(datar_block.var, 1);
    
    run_process(); 
    
    PQfinish(connection);    

    sem_close(sem_lock_datar.var);
    sem_close(sem_lock_commr.var);
    sem_close(sem_lock_sigr.var);
    
    detach_memory_block(datar_block.var);
    detach_memory_block(commr_block.var);
    
    destroy_memory_block(datar_block.var, datar_block.key);
    destroy_memory_block(commr_block.var, commr_block.key);
    
    return 0;
}   