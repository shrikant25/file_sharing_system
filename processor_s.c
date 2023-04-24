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

int process_status = 1;

datablocks dblks;
semlocks smlks;

PGconn *connection;


int retrive_data_from_database(char *blkptr) 
{
    int row_count;
    int status = -1;
    char error[100];
    PGresult *res = NULL;
    send_message *sndmsg = (send_message *)blkptr;

    res = PQexecPrepared(connection, dbs[0].statement_name, dbs[0].param_count, 
                                    NULL, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        memset(error, 0, sizeof(error));
        sprintf(error, "%s %s", "failed to retrive data from db", PQerrorMessage(connection));
        store_log(error);
    }    
    else {

        row_count = PQntuples(res);
        if (row_count > 0) {
        
            sndmsg->fd = atoi(PQgetvalue(res, 0, 0));
            strncpy(sndmsg->uuid, PQgetvalue(res, 0, 1), PQgetlength(res, 0, 1));
            strncpy(sndmsg->data, PQgetvalue(res, 0, 2), PQgetlength(res, 0, 2));
            PQclear(res);

            const char *const param_values[] = {sndmsg->uuid};
            const int paramLengths[] = {sizeof(sndmsg->uuid)};
            const int paramFormats[] = {0};
            int resultFormat = 0;
            
            res = PQexecPrepared(connection, dbs[4].statement_name, dbs[4].param_count, 
                                    param_values, paramLengths, paramFormats, resultFormat);
            if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                memset(error, 0, sizeof(error));
                sprintf(error, "%s %s", "failed update job status", PQerrorMessage(connection));
                store_log(error);
                status  = -1;
            }
            else{
                status = 0;
            }
        }
    }

    PQclear(res);
    return status;   
}


int store_comms_into_database(char *blkptr) 
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


int retrive_comms_from_database(char *blkptr) 
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


void store_log(char *logtext) 
{

    PGresult *res = NULL;
    char log[100];
    strncpy(log, logtext, strlen(logtext));

    const char *const param_values[] = {log};
    const int paramLengths[] = {sizeof(log)};
    const int paramFormats[] = {0};
    int resultFormat = 0;
    
    res = PQexecPrepared(connection, "ps_storelog", 5, param_values, paramLengths, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        syslog(LOG_NOTICE, "logging failed %s , log %s\n", PQerrorMessage(connection), log);
    }

    PQclear(res);
}


int give_data_to_sender() 
{
    int subblock_position = -1;
    char *blkptr = NULL;

    sem_wait(smlks.sem_lock_datas);         
    subblock_position = get_subblock(dblks.datas_block, 0, 3);
    
    if (subblock_position >= 0) {

        blkptr = dblks.datas_block + (TOTAL_PARTITIONS/8) + subblock_position * DPARTITION_SIZE;
        
        if (retrive_data_from_database(blkptr) != -1) { 
            toggle_bit(subblock_position, dblks.datas_block, 3);
            sem_post(smlks.sem_lock_sigs);
        }
    
    }

    sem_post(smlks.sem_lock_datas);
}


int send_msg_to_sender() 
{
    int subblock_position = -1;
    char *blkptr = NULL;

    sem_wait(smlks.sem_lock_comms);         
    subblock_position = get_subblock(dblks.comms_block, 0, 1);
    
    if (subblock_position >= 0) {

        blkptr = dblks.comms_block + (TOTAL_PARTITIONS/8) + subblock_position*CPARTITION_SIZE;
        
        if (retrive_comms_from_database(blkptr) != -1){
            toggle_bit(subblock_position, dblks.comms_block, 1);
            sem_post(smlks.sem_lock_sigs);
        }
    }
    sem_post(smlks.sem_lock_comms);

}


int read_msg_from_sender() 
{

    int subblock_position = -1;
    char *blkptr = NULL;

    sem_wait(smlks.sem_lock_comms);         
    subblock_position = get_subblock(dblks.comms_block, 1, 2);
    
    if (subblock_position >= 0) {

        blkptr = dblks.comms_block + (TOTAL_PARTITIONS/8) + subblock_position*CPARTITION_SIZE;
        store_comms_into_database(blkptr);
        toggle_bit(subblock_position, dblks.comms_block, 2);
    
    }
    sem_post(smlks.sem_lock_comms);   
}


int run_process() 
{
   
    while (process_status) {

        sem_wait(smlks.sem_lock_sigps);
        give_data_to_sender();
        send_msg_to_sender();
        read_msg_from_sender();
        
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
    int pid = fork();
    if (pid < 0) {
        syslog(LOG_NOTICE, "failed to fork %s", strerror(errno));
        return -1;
    }
    else if(pid == 0) {

        const char *path = "/home/shrikant/Desktop/prj/sender_notif";
        char *params[] = {"sender_notif"};
        int result;
        result = execv(path, params);
        if (result == -1) {
            syslog(LOG_NOTICE, "failed execv sender_notif  %s", strerror(errno));
        }
        exit(0);
    }
    else {

        int status = 0;
        if (connect_to_database() == -1) { return -1; }
        if (prepare_statements() == -1) { return -1; }   
    
        sem_unlink(SEM_LOCK_DATAS);
        sem_unlink(SEM_LOCK_COMMS);
        sem_unlink(SEM_LOCK_SIG_S); 
        sem_unlink(SEM_LOCK_SIG_PS);

        smlks.sem_lock_datas = sem_open(SEM_LOCK_DATAS, O_CREAT, 0777, 1);
        smlks.sem_lock_comms = sem_open(SEM_LOCK_COMMS, O_CREAT, 0777, 1);
        smlks.sem_lock_sigs = sem_open(SEM_LOCK_SIG_S, O_CREAT, 0777, 0);
        smlks.sem_lock_sigps = sem_open(SEM_LOCK_SIG_PS, O_CREAT, 0777, 0);

        if (smlks.sem_lock_sigps == SEM_FAILED || smlks.sem_lock_sigs == SEM_FAILED || smlks.sem_lock_datas == SEM_FAILED || smlks.sem_lock_comms == SEM_FAILED) {
            store_log("failed to initialize locks");
            return -1;
        }
            
        dblks.datas_block = attach_memory_block(FILENAME_S, DATA_BLOCK_SIZE, (unsigned char)PROJECT_ID_DATAS);
        dblks.comms_block = attach_memory_block(FILENAME_S, COMM_BLOCK_SIZE, (unsigned char)PROJECT_ID_COMMS);

        if (!( dblks.datas_block && dblks.comms_block)) {
            store_log("failed to get shared memory");
            return -1; 
        }

        unset_all_bits(dblks.comms_block, 2);
        unset_all_bits(dblks.comms_block, 3);
        unset_all_bits(dblks.datas_block, 1);
        
        run_process(); 
        PQfinish(connection);  

        sem_close(smlks.sem_lock_datas);
        sem_close(smlks.sem_lock_comms);
        sem_close(smlks.sem_lock_sigs);
        sem_close(smlks.sem_lock_sigps);

        detach_memory_block(dblks.datas_block);
        detach_memory_block(dblks.comms_block);

        destroy_memory_block(dblks.datas_block, PROJECT_ID_DATAS);
        destroy_memory_block(dblks.comms_block, PROJECT_ID_COMMS);

        return 0;
    }
}   