#include <libpq-fe.h>
#include <string.h>
#include <stdio.h>
#include "partition.h"
#include "processor_db.h"
#include "message.h"

PGconn *connection;

#define statement_count 11

db_statements dbs[statement_count] = {
    { 
      .statement_name = "s0",  
      .statement = "INSERT INTO job_scheduler(jobdata, jstate, jtype,\
                    jsource_ip, jobid, jparent_jobid, jdestination_ip,\
                    jpriority) VALUES($2, 'N-0', 0, $1, GEN_RANDOM_UUID(),\
                    (select jobid from job_scheduler where jidx = 1), 0, 0);",
      .param_count = 2,
    },
    { 
      .statement_name = "s1", 
      .statement = "SELECT * from sysinfo;",
      .param_count = 0,
    },
    { 
      .statement_name = "s2", 
      .statement = "INSERT INTO receiving_conns (rfd, ripaddr, rcstatus) VALUES ($1, $2, $3) ON CONFLICT (rfd) DO UPDATE SET rcstatus = ($3);",
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
      .statement = "select * from sysinfo;",
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


int store_data_in_database(rcondata *rcond) 
{
    PGresult *res = NULL;
    
    char fd[11];

    sprintf(fd, "%d", rcond->fd);

    const int paramLengths[] = {sizeof(fd), sizeof(rcond->data)};
    const int paramFormats[] = {1, 1};
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
        printf("retriving failed: %s\n", PQerrorMessage(connection));
        return -1;
    }    

    row_count = PQntuples(res);
    if (row_count > 0) {
        memcpy(data, PQgetvalue(res, 0, 0), PQgetlength(res, 0, 0));
        memcpy(data+4, PQgetvalue(res, 0, 1), PQgetlength(res, 0, 1));
        memcpy(data+8, PQgetvalue(res, 0, 2), PQgetlength(res, 0, 2));
        PQclear(res);
        return 0;
    }

    PQclear(res);
    return -1;   
}


int store_commr_into_database(rconmsg *rcvm) 
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

/*
int store_comms_into_database(char *data) 
{
    char msg_id[17];
    char msg_status[5]; 
    unsigned char *ptr = data;
    PGresult* res = NULL;

    if (*ptr == 1) {

        ptr++;
        memcpy(msg_id, ptr, 16);   

        ptr += 16;
        sprintf(msg_status, "%u", *(int *)ptr);

        const char *param_values[] = {msg_id, msg_status};
        const int paramLengths[] = {sizeof(msg_id), sizeof(msg_status)};
        const int paramFormats[] = {0, 0};
        int resultFormat = 0;

        res = PQexecPrepared(connection, dbs[3].statement_name, dbs[3].param_count, param_values, paramLengths, paramFormats, resultFormat);

    } 

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(connection));
        return -1;
    }

    PQclear(res);

    return 0;
}

*/
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

/*
int retrive_comms_from_database(char *data) 
{
    char cid[4];
    int row_count = 0;
    PGresult *res = NULL;

    res = PQexecPrepared(connection, dbs[5].statement_name, dbs[5].param_count, NULL, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("retriving failed: %s\n", PQerrorMessage(connection));
        return -1;
    }    

    row_count = PQntuples(res);
    if (row_count > 0) {
    
        memcpy(data, PQgetvalue(res, 0, 0), PQgetlength(res, 0, 0));
        memcpy(cid, data, 4);
        memcpy(data+4, PQgetvalue(res, 0, 1), PQgetlength(res, 0, 1));

        PQclear(res);
    
        // update this row from database
        const char *param_values[] = {cid};
        const int paramLengths[] = {sizeof(cid)};
        const int paramFormats[] = {0};
        int resultFormat = 0;     
        
        res = PQexecPrepared(connection, dbs[6].statement_name, dbs[6].param_count, param_values, paramLengths, paramFormats, resultFormat);

        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            printf("updation after retriving failed: %s\n", PQerrorMessage(connection));
            PQclear(res);
   
            return -1;
        }   

        PQclear(res);

        return 0;
    
    }

    PQclear(res);
    return -1;

}

*/
int close_database_connection() 
{   
    PQfinish(connection);
    return 0;
}