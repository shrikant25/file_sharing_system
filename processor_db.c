#include <libpq-fe.h>
#include <string.h>
#include <stdio.h>
#include "partition.h"
#include "processor_db.h"
#include <syslog.h>
#include <stdlib.h>

PGconn *connection;

#define statement_count 11

db_statements dbs[statement_count] = {
    { 
      .statement_name = "s0",  
      .statement = "INSERT INTO job_scheduler(jobdata, jstate, jtype, \
                    jsource, jobid, jparent_jobid, jdestination, \
                    jpriority) VALUES($2, 'N-0', '0', $1, GEN_RANDOM_UUID(), \
                    (select jobid from job_scheduler where jidx = 1), 0, 0);",
      .param_count = 2,
    },
    { 
      .statement_name = "s1", 
      .statement = "SELECT jobdata FROM job_scheduler js \
                    WHERE js.jstate = 'S-3' AND \
                    EXISTS (SELECT  1 FROM sending_conns sc \
                    WHERE sc.sipaddr = js.jdestination \
                    AND sc.scstatus = '2') \
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
      .statement = "select * from sysinfo;",
      .param_count = 0,
    },
    { 
      .statement_name = "s4", 
      .statement = "select * from sysinfo;",
      .param_count = 0,
    },
    { 
      .statement_name = "s5", 
      .statement = "WITH sdata AS(SELECT scommid FROM senders_comms WHERE mtype IN(1, 2) LIMIT 1) \
                    DELETE FROM senders_comms WHERE scommid = (SELECT scommid FROM sdata) RETURNING mtype, mdata;",
      .param_count = 0,
    },
    { 
      .statement_name = "s6", 
      .statement = "select * from sysinfo;",
      .param_count = 0,
    },
    { 
      .statement_name = "s7", 
      .statement = "select * from sysinfo;",
      .param_count = 0,
    },
    { 
      .statement_name = "s8", 
      .statement = "select * from sysinfo;",
      .param_count = 0,
    },
    { 
      .statement_name = "s9", 
      .statement = "select * from sysinfo;",
      .param_count = 0,
    },
    { 
      .statement_name = "s10", 
      .statement = "select * from sysinfo;",
      .param_count = 1,
    },
};

//if any error occurs try to mitigate it here only
//if mitigation fails then return -1
int connect_to_database() 
{   
    connection = PQconnectdb("user=shrikant dbname=shrikant");
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


int store_data_in_database(newmsg_data *rcond) 
{
    PGresult *res = NULL;
    
    char fd[11];

    sprintf(fd, "%d", rcond->fd);

    const int paramLengths[] = {sizeof(fd), sizeof(rcond->data)};
    const int paramFormats[] = {0, 1};
    int resultFormat = 0;

    const char *const param_values[] = {fd, rcond->data};
    
    res = PQexecPrepared(connection, dbs[0].statement_name, 
                                    dbs[0].param_count, param_values, paramLengths, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(connection));
        return -1;
    }

    PQclear(res);

    return 0;
}


int retrive_data_from_database(char *data) 
{
    int row_count;
    PGresult *res = NULL;

    res = PQexecPrepared(connection, dbs[1].statement_name, dbs[1].param_count, 
                                    NULL, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        syslog(LOG_NOTICE,"retriving failed: %s", PQerrorMessage(connection));
        return -1;
    }    

    row_count = PQntuples(res);
    if (row_count > 0) {
        memcpy(data, PQgetvalue(res, 0, 0), PQgetlength(res, 0, 0));
        memcpy(data+4, PQgetvalue(res, 0, 1), PQgetlength(res, 0, 1));
        PQclear(res);
        return 0;
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

    const int paramLengths[] = {sizeof(fd), sizeof(ipaddr), sizeof(ipaddr)};
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


int store_comms_into_database(senders_message *smsg) 
{
    PGresult* res = NULL;
    char type[5];
    char data1[10];
    char data2[10];

    sprintf(type, "%d", smsg->type);
    sprintf(data1, "%d", smsg->data1);

    if (smsg->type == 3) {    

        sprintf(data2, "%d", smsg->data2);

        const char *param_values[] = {data1, data2};
        const int paramLengths[] = {strlen(data1), strlen(data2)};
        const int paramFormats[] = {0, 0, 0};
        int resultFormat = 0;

        res = PQexecPrepared(connection, dbs[3].statement_name, dbs[3].param_count, param_values, paramLengths, paramFormats, resultFormat);
    }
    else if(smsg->type == 4) {
        // todo message status is type 4
    }

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(connection));
        return -1;
    }

    PQclear(res);

    return 0;
}


/*
int retrive_commr_from_database(char *data) 
{
    int row_count = 0;
    PGresult *res = NULL;

    res = PQexecPrepared(connection, dbs[4].statement_name, dbs[4].param_count, NULL, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("retriving failed: %s\n", PQerrorMessage(connection));
        return -1;
    }    

    row_count = PQntuples(res);
    if (row_count > 0) {
        memcpy(data, PQgetvalue(res, 0, 0), PQgetlength(res, 0, 0));
        PQclear(res);
        return 0;
    }

    PQclear(res);
    return -1;
}
*/

int retrive_comms_from_database(senders_message *smsg) 
{
    int row_count = 0;
    PGresult *res = NULL;
    int status = -1;

    res = PQexecPrepared(connection, dbs[5].statement_name, dbs[5].param_count, NULL, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        syslog(LOG_NOTICE,"retriving failed: %s\n", PQerrorMessage(connection));
        return status;
    }    

    row_count = PQntuples(res);
    if (row_count > 0) {
    
        smsg->type = atoi(PQgetvalue(res, 0, 0));
        if (smsg->type == 1) { 
           smsg->data1 = atoi(PQgetvalue(res, 0, 1));
           smsg->data2 = atoi(PQgetvalue(res, 0, 2));
        }
        else if(smsg->type == 2) {
            smsg->data1 = atoi(PQgetvalue(res, 0, 1));
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