#include <libpq-fe.h>
#include <string.h>
#include <stdio.h>
#include "partition.h"
#include "processor_db.h"

PGconn *connection;

#define statement_count 9

db_statements dbs[statement_count] = {
    { 
      .statement_name = "s0", 
      .statement = "INSERT INTO raw_data (fd, data, data_size) VALUES ($1, $2, $3);",
      .param_count = 3,
    },
    { 
      .statement_name = "s1", 
      .statement = "SELECT fd, data, data_size FROM send_data LIMIT 1;",
      .param_count = 0,
    },
    { 
      .statement_name = "s2", 
      .statement = "INSERT INTO open_connections_receiving (fd, ipaddr, status) VALUES ($1, $2, $3);",
      .param_count = 3,
    },
    { 
      .statement_name = "s3", 
      .statement = "INSERT INTO senders_comm (msgid, status) VALUES ($1, $2);",
      .param_count = 2,
    },
    { 
      .statement_name = "s4", 
      .statement = "SELECT fd FROM for_receiver WHERE status = 1 LIMIT 1;",
      .param_count = 0,
    },
    { 
      .statement_name = "s5", 
      .statement = "SELECT cid, ipaddr FROM for_sender WHERE status = 1 LIMIT 1;",
      .param_count = 0,
    },
    { 
      .statement_name = "s6", 
      .statement = "UPDATE for_sender SET status = 2 WHERE cid = ($1);",
      .param_count = 1,
    },
    { 
      .statement_name = "s7", 
      .statement = "UPDATE for_sender SET status = 3 WHERE cid = ($1);",
      .param_count = 1,
    },
    { 
      .statement_name = "s8", 
      .statement = "UPDATE open_connections SET status = 0 WHERE fd = ($1);",
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


int store_data_in_database(char *data) 
{
    PGresult *res = NULL;
    
    char cfd[4];
    char cdata_size[4];
    char cdata[1024*128];

    memset(cfd, 0 , sizeof(cfd));
    sprintf(cfd, "%d", *(int*)data);
    data += 4;

    memset(cdata_size, 0, sizeof(cdata_size));
    sprintf(cdata_size, "%d", *(int *)data);

    memset(cdata, 0, sizeof(cdata));
    memcpy(cdata, data+4, *(int *)data);

    const int paramLengths[] = {sizeof(cfd), *(int *)data, sizeof(cdata_size)};
    const int paramFormats[] = {0, 0, 0};
    int resultFormat = 0;

    const char *const param_values[] = {cfd, cdata, cdata_size};
    
    
    res = PQexecPrepared(connection, dbs[0].statement_name, 
                                    dbs[0].param_count, param_values, NULL, NULL, 0);
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


// if first byte is denotes values 1, then it is data containing fd,ip and port
int store_commr_into_database(char *data) 
{
    char fd[10];
    char ip_addr[33];
    char status[2];
    unsigned char *ptr = data;
    
    PGresult *res = NULL;

    if (*ptr == '1') {
        
        memset(status, 0, sizeof(status));
        memset(fd, 0, sizeof(fd));
        memset(ip_addr, 0, sizeof(ip_addr));

        memcpy(status, ptr, 1);

        ptr++;
        sprintf(fd, "%u", *(int *)ptr);
        
        ptr+=4;
        sprintf(ip_addr, "%u", *(int *)ptr);

        const int paramLengths[] = {sizeof(fd), sizeof(ip_addr), sizeof(status)};
        const int paramFormats[] = {0, 0, 0};
        int resultFormat = 0;
        
        const char *const param_values[] = {fd, ip_addr, status};
        
        res = PQexecPrepared(connection, dbs[2].statement_name, dbs[2].param_count, param_values, paramLengths, paramFormats, resultFormat);

    }
    else if (*ptr == '2') { // this updated statement can be combined with insert
        
        ptr++;
        sprintf(fd, "%u", *(int *)ptr);
       
        const char *param_values[] = {fd};
        
        res = PQexecPrepared(connection, dbs[8].statement_name, dbs[8].param_count, param_values, NULL, NULL, 0);
        
    }

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("message storing failed failed: %s\n", PQerrorMessage(connection));
        return -1;
    }

    PQclear(res);

    return 0;
}


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


int close_database_connection() 
{   
    PQfinish(connection);
    return 0;
}