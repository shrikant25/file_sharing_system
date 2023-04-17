#include <libpq-fe.h>
#include <string.h>
#include <stdio.h>
#include "partition.h"
#include "processor_db.h"
#include <syslog.h>
#include <stdlib.h>

PGconn *connection;

#define statement_count 6

db_statements dbs[statement_count] = {
    { 
      .statement_name = "s0",  
      .statement = "INSERT INTO job_scheduler(jobdata, jstate, jtype, \
                    jsource, jobid, jparent_jobid, jdestination, \
                    jpriority) VALUES($2, 'N-0', '0', $1, GEN_RANDOM_UUID(), \
                    (select jobid from job_scheduler where jparent_jobid = jobid), 0, 0);",
      .param_count = 2,
    },
    { 
      .statement_name = "s1", 
      .statement = "SELECT sc.sfd, js.jobid, js.jobdata\
                    FROM job_scheduler js, sending_conns sc \
                    WHERE js.jstate = 'S-3' \
                    AND sc.sipaddr::text = js.jdestination \
                    AND sc.scstatus = 2 \
                    ORDER BY jpriority DESC LIMIT 1;",
      .param_count = 0,
    },
    { 
      .statement_name = "s2", 
      .statement = "INSERT INTO receiving_conns (rfd, ripaddr, rcstatus) \
                    VALUES ($1, $2, $3) ON CONFLICT (rfd) DO UPDATE SET rcstatus = ($3);",
      .param_count = 3,
    },
    { 
      .statement_name = "s3", 
      .statement = "UPDATE sending_conns SET scstatus = ($3), sfd = ($2)  WHERE sipaddr = ($1);",
      .param_count = 3,
    },
    { 
      .statement_name = "s4", 
      .statement = "UPDATE job_scheduler \
                    SET jstate = (\
                        SELECT\
                            CASE\
                                WHEN ($2) > 0 THEN 'C'\
                                ELSE 'D'\
                            END\
                        )\
                    WHERE jobid = $1::uuid;",
      .param_count = 2,
    },
    { 
      .statement_name = "s5", 
      .statement = "WITH sdata AS(SELECT scommid FROM senders_comms WHERE mtype IN(1, 2) LIMIT 1) \
                    DELETE FROM senders_comms WHERE scommid = (SELECT scommid FROM sdata) RETURNING mtype, mdata1, mdata2;",
      .param_count = 0,
    },
};

//if any error occurs try to mitigate it here only
//if mitigation fails then return -1
int connect_to_database() 
{   
    connection = PQconnectdb("user = shrikant dbname = shrikant");
    if (PQstatus(connection) == CONNECTION_BAD) {
        fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(connection));
        // mitigate
        //if fails then return -1
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
            printf("Preparation of statement failed: %s\n", PQerrorMessage(connection));
            return -1;
        }

        PQclear(res);
    }

    return 0;
}


int store_data_in_database(newmsg_data *nmsg) 
{
    PGresult *res = NULL;
    
    char fd[11];

    sprintf(fd, "%d", nmsg->data1);

    const int paramLengths[] = {sizeof(nmsg->data1), sizeof(nmsg->data)};
    const int paramFormats[] = {0, 1};
    int resultFormat = 0;

    const char *const param_values[] = {fd, nmsg->data};
    
    res = PQexecPrepared(connection, dbs[0].statement_name, 
                                    dbs[0].param_count, param_values, paramLengths, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(connection));
        return -1;
    }

    PQclear(res);

    return 0;
}


int retrive_data_from_database(char *blkptr) 
{
    int row_count;
    int status = -1;
    PGresult *res = NULL;
    send_message *sndmsg = (send_message *)blkptr;

    res = PQexecPrepared(connection, dbs[1].statement_name, dbs[1].param_count, 
                                    NULL, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        syslog(LOG_NOTICE,"retriving failed: %s", PQerrorMessage(connection));
    }    
    else {

        row_count = PQntuples(res);
        if (row_count > 0) {
            sndmsg->fd = atoi(PQgetvalue(res, 0, 0));
            strncpy(sndmsg->uuid, PQgetvalue(res, 0, 1), PQgetlength(res, 0, 1));
            strncpy(sndmsg->data, PQgetvalue(res, 0, 2), PQgetlength(res, 0, 2));
            return 0;
        }
    }
    PQclear(res);
    return -1;   
}


int store_commr_into_database(receivers_message *rcvm) 
{
    PGresult *res = NULL;

    char fd[11];
    char ipaddr[11];
    char status [11];

    sprintf(fd, "%d", rcvm->fd);
    sprintf(ipaddr, "%d", rcvm->ipaddr);
    sprintf(status, "%d", rcvm->status);

    const int paramLengths[] = {sizeof(fd), sizeof(ipaddr), sizeof(status)};
    const int paramFormats[] = {0, 0, 0};
    int resultFormat = 0;
   
    const char *const param_values[] = {fd, ipaddr, status};
    
    res = PQexecPrepared(connection, dbs[2].statement_name, dbs[2].param_count, param_values, paramLengths, paramFormats, resultFormat);
   
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("message storing failed failed: %s\n", PQerrorMessage(connection));
        return -1;
    }

    PQclear(res);

    return 0;
}


int store_comms_into_database(char *blkptr) 
{
    int resultFormat = 0;
    char ipaddress[11];
    char fd[11];
    char status[11];
    char uuid[37];

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
        
        res = PQexecPrepared(connection, dbs[3].statement_name, dbs[3].param_count, param_values, paramLengths, paramFormats, resultFormat);
  
    }
    else if(*(unsigned char *)blkptr == 4) {
        
        message_status *msgsts = (message_status *)blkptr;
        
        sprintf(status, "%hhu", msgsts->status);
        strncpy(uuid, msgsts->uuid, sizeof(msgsts->uuid));
        

        const char *param_values[] = {uuid, status};
        const int paramLengths[] = {sizeof(uuid), sizeof(status)};
        const int paramFormats[] = {0, 0};
        res = PQexecPrepared(connection, dbs[4].statement_name, dbs[4].param_count, param_values, paramLengths, paramFormats, resultFormat);
    
    }

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Insert of communication failed: %s\n", PQerrorMessage(connection));
        return -1;
    }

    PQclear(res);

    return 0;
}


int retrive_commr_from_database(char *data) 
{
    // int row_count = 0;
    // PGresult *res = NULL;

    // res = PQexecPrepared(connection, dbs[4].statement_name, dbs[4].param_count, NULL, NULL, NULL, 0);

    // if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    //     printf("retriving failed: %s\n", PQerrorMessage(connection));
    //     return -1;
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


int retrive_comms_from_database(char *blkptr) 
{
    PGresult *res = NULL;
    int status = -1;
    int type;

    res = PQexecPrepared(connection, dbs[5].statement_name, dbs[5].param_count, NULL, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        syslog(LOG_NOTICE,"retriving failed: %s\n", PQerrorMessage(connection));
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


int close_database_connection() 
{   
    PQfinish(connection);
    return 0;
}