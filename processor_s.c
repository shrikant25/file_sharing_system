#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libpq-fe.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/shm.h>
#include "shared_memory.h"
#include "partition.h"
#include "processor_s.h"

semlocks sem_lock_datas;
semlocks sem_lock_comms;
semlocks sem_lock_sigs;
semlocks sem_lock_sigps;
datablocks datas_block;
datablocks comms_block;
PGconn *connection;


int retrive_data_from_database (char *blkptr) 
{
    int row_count;
    int status = -1;
    char error[100];
    PGresult *res = NULL;
    send_message *sndmsg = (send_message *)blkptr;

    res = PQexecPrepared(connection, dbs[0].statement_name, dbs[0].param_count, 
                                    NULL, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) <= 0) {
        memset(error, 0, sizeof(error));
        sprintf(error, "failed to retrive info from db %s", PQerrorMessage(connection));
        store_log(error);
    }    
    else {
        
        sndmsg->fd = atoi(PQgetvalue(res, 0, 0));
        strncpy(sndmsg->uuid, PQgetvalue(res, 0, 1), PQgetlength(res, 0, 1));
        PQclear(res);

        const char *const param_values[] = {sndmsg->uuid};
        const int paramLengths[] = {sizeof(sndmsg->uuid)};
        const int paramFormats[] = {0};
        int resultFormat = 1;

        res = PQexecPrepared(connection, dbs[6].statement_name, dbs[6].param_count, param_values,   
                                        paramLengths, paramFormats, resultFormat);
        
        if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) <= 0) {
            memset(error, 0, sizeof(error));
            sprintf(error, "failed to retrive data from db %s", PQerrorMessage(connection));
            store_log(error);
        }
        else {

            strncpy(sndmsg->data, PQgetvalue(res, 0, 0), PQgetlength(res, 0, 0));
            PQclear(res);

            res = PQexecPrepared(connection, dbs[4].statement_name, dbs[4].param_count, 
                                    param_values, paramLengths, paramFormats, resultFormat);

            if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                memset(error, 0, sizeof(error));
                sprintf(error, "failed update job status %s", PQerrorMessage(connection));
                store_log(error);
            }
            else {
                status = 0;
            }
        }
    }

    PQclear(res);
    return status;   
}


int store_comms_into_database (char *blkptr) 
{
    int resultFormat = 0;
    char ipaddress[11];
    char fd[11];
    char status[11];
    char uuid[37];
    char error[100];

    PGresult* res = NULL;
    
    if(*(unsigned char *)blkptr == 3){
        
        connection_status *cncsts = (connection_status *)blkptr;
        sprintf(ipaddress, "%d", cncsts->ipaddress);        
        sprintf(fd, "%d", cncsts->fd);

        if (cncsts->fd >= 0) {
            sprintf(status, "%d", 2);
        }

        const char *param_values[] = {ipaddress, fd, status};
        const int paramLengths[] = {sizeof(ipaddress), sizeof(fd), sizeof(status)};
        const int paramFormats[] = {0, 0, 0};
        
        res = PQexecPrepared(connection, dbs[1].statement_name, dbs[1].param_count, param_values, paramLengths, paramFormats, resultFormat);
  
    }
    else if(*(unsigned char *)blkptr == 4) {
        
        message_status *msgsts = (message_status *)blkptr;
        
        sprintf(status, "%hhu", msgsts->status);
        strncpy(uuid, msgsts->uuid, sizeof(msgsts->uuid));
        

        const char *param_values[] = {uuid, status};
        const int paramLengths[] = {sizeof(uuid), sizeof(status)};
        const int paramFormats[] = {0, 0};
        res = PQexecPrepared(connection, dbs[2].statement_name, dbs[2].param_count, param_values, paramLengths, paramFormats, resultFormat);
    
    }

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        memset(error, 0, sizeof(error));
        sprintf(error, "%s %s", "failed to insert senders comms", PQerrorMessage(connection));
        store_log(error);
        return -1;
    }

    PQclear(res);

    return 0;
}


int retrive_comms_from_database (char *blkptr) 
{
    PGresult *res = NULL;
    int status = -1;
    char error[100];
    int type;

    res = PQexecPrepared(connection, dbs[3].statement_name, dbs[3].param_count, NULL, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        memset(error, 0, sizeof(error));
        sprintf(error, "%s %s", "retriving comms for sender failed", PQerrorMessage(connection));
        store_log(error);
        return status;
    }    

    if (PQntuples(res) > 0) {
        
        type = atoi(PQgetvalue(res, 0, 0));

        if (type == 1){
           
            open_connection *opncn = (open_connection *)blkptr;
            opncn->type = atoi(PQgetvalue(res, 0, 0));
            opncn->ipaddress = atoi(PQgetvalue(res, 0, 1)); 
            opncn->port = atoi(PQgetvalue(res, 0, 2));
            
        }
        else if(type == 2) {
            
            close_connection *clscn = (close_connection *)blkptr;
            clscn->type = atoi(PQgetvalue(res, 0, 0));
            clscn->fd = atoi(PQgetvalue(res, 0, 1));
        }

        status = 0;

    }

    PQclear(res);
    return status;
}


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
    
    res = PQexecPrepared(connection, dbs[5].statement_name, dbs[5].param_count, param_values, paramLengths, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        syslog(LOG_NOTICE, "logging failed %s , log %s\n", PQerrorMessage(connection), log);
    }

    PQclear(res);
}


int give_data_to_sender () 
{
    int subblock_position = -1;
    char *blkptr = NULL;

    sem_wait(sem_lock_datas.var);         
    subblock_position = get_subblock(datas_block.var, 0, 3);
    
    if (subblock_position >= 0) {

        blkptr = datas_block.var + (TOTAL_PARTITIONS/8) + subblock_position * DPARTITION_SIZE;
        
        if (retrive_data_from_database(blkptr) != -1) { 
            toggle_bit(subblock_position, datas_block.var, 3);
            sem_post(sem_lock_sigs.var);
        }
    
    }

    sem_post(sem_lock_datas.var);
}


int send_msg_to_sender () 
{
    int subblock_position = -1;
    char *blkptr = NULL;

    sem_wait(sem_lock_comms.var);         
    subblock_position = get_subblock(comms_block.var, 0, 1);
    
    if (subblock_position >= 0) {

        blkptr = comms_block.var + (TOTAL_PARTITIONS/8) + subblock_position*CPARTITION_SIZE;
        
        if (retrive_comms_from_database(blkptr) != -1){
            toggle_bit(subblock_position, comms_block.var, 1);
            sem_post(sem_lock_sigs.var);
        }
    }
    sem_post(sem_lock_comms.var);

}


int read_msg_from_sender () 
{

    int subblock_position = -1;
    char *blkptr = NULL;

    sem_wait(sem_lock_comms.var);         
    subblock_position = get_subblock(comms_block.var, 1, 2);
    
    if (subblock_position >= 0) {

        blkptr = comms_block.var + (TOTAL_PARTITIONS/8) + subblock_position*CPARTITION_SIZE;
        store_comms_into_database(blkptr);
        toggle_bit(subblock_position, comms_block.var, 2);
    
    }
    sem_post(sem_lock_comms.var);   
}


int run_process () 
{
    while (1) {

        sem_wait(sem_lock_sigps.var);
        send_msg_to_sender();
        read_msg_from_sender();
        give_data_to_sender();
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
    
        sscanf(buf, "SEM_LOCK_DATAS=%s\nSEM_LOCK_COMMS=%s\nSEM_LOCK_SIG_S=%s\nSEM_LOCK_SIG_PS=%s\nPROJECT_ID_DATAS=%d\nPROJECT_ID_COMMS=%d\nUSERNAME=%s\nDBNAME=%s", sem_lock_datas.key, sem_lock_comms.key, sem_lock_sigs.key, sem_lock_sigps.key, &datas_block.key, &comms_block.key, username, dbname);
    }
    else {
        syslog(LOG_NOTICE, "failed to read configuration file");
        return -1;
    }

    close(conffd);

    sprintf(db_conn_command, "user=%s dbname=%s", username, dbname);
    if (connect_to_database(db_conn_command) == -1) { return -1; }
    if (prepare_statements() == -1) { return -1; }   
    
    sem_lock_datas.var = sem_open(sem_lock_datas.key, O_CREAT, 0777, 1);
    sem_lock_comms.var = sem_open(sem_lock_comms.key, O_CREAT, 0777, 1);
    sem_lock_sigs.var = sem_open(sem_lock_sigs.key, O_CREAT, 0777, 0);
    sem_lock_sigps.var = sem_open(sem_lock_sigps.key, O_CREAT, 0777, 0);

    if (sem_lock_sigps.var == SEM_FAILED || sem_lock_sigs.var == SEM_FAILED || sem_lock_datas.var == SEM_FAILED || sem_lock_comms.var == SEM_FAILED) {
        store_log("failed to initialize locks");
        return -1;
    }
        
    datas_block.var = attach_memory_block(FILENAME_S, DATA_BLOCK_SIZE, (unsigned char)datas_block.key);
    comms_block.var = attach_memory_block(FILENAME_S, COMM_BLOCK_SIZE, (unsigned char)comms_block.key);

    if (!( datas_block.var && comms_block.var)) {
        store_log("failed to get shared memory");
        return -1; 
    }

    unset_all_bits(comms_block.var, 2);
    unset_all_bits(comms_block.var, 3);
    unset_all_bits(datas_block.var, 1);
    
    run_process(); 
    PQfinish(connection);  

    sem_close(sem_lock_datas.var);
    sem_close(sem_lock_comms.var);
    sem_close(sem_lock_sigs.var);
    sem_close(sem_lock_sigps.var);

    detach_memory_block(datas_block.var);
    detach_memory_block(comms_block.var);

    destroy_memory_block(datas_block.var, datas_block.key);
    destroy_memory_block(comms_block.var, comms_block.key);

    return 0;

}   