#include <libpq-fe.h>
#include <string.h>
#include <stdio.h>
#include "partition.h"
#include "processor_db.h"


PGconn *connection;

#define statement_count  8

char *statement_names[statement_count] = {
                            "s0" // store data in database ;
                            "s1" // retrive data from_database;
                            "s2" // store (commr received from receiver) into database;
                            "s3" // store (comms received from senders) into database;
                            "s4" // retrive commr from database intended for receiver;
                            "s5" // retrive comms from database intended for sender;
                            "s6" // update for_sender table set status as 2 i.e "connection establishment in progress";
                            "s7" // update for_sender table set status as 3 i.e "connection established"
                          };


char *statements[statement_count] = {
                    "INSERT INTO raw_data (fd, data, data_size) values($1, $2, $3)",
                    "SELECT fd, data, data_size FROM send_data limit 1",
                    "INSERT INTO open_connections (fd, ipaddr) VALUES($1, $2)",
                    "INSERT INTO  senders_comm (msgid, status) VALUES($1, $2)",
                    "SELECT fd FROM for_receiver where status = 1 limit 1",
                    "SELECT cid, ipaddr FROM for_sender where status = 1 limit 1",
                    "UPDATE for_sender set status = 2 where cid = ($1)",
                    "UPDATE for_sender set status = 3 where cid = ($1)",
                    }; 

int param_count[statement_count] = { 3, 0, 2, 2, 0, 0, 1, 1};


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


int prepare_statments() 
{    
    int i;

    for(i = 0; i<statement_count; i++){

        PGresult* res = PQprepare(connection, statement_names[i], 
                                    statements[i], param_count[i], NULL);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            printf("Preparation of statement failed: %s\n", PQerrorMessage(connection));
        }

        PQclear(res);
    }

    return 0;
}


int store_data_in_database(int fd, char *data, int data_size) 
{
    PGresult *res = NULL;
    const char *param_values[param_count[0]];
    param_values[0] = data;

    res = PQexecPrepared(connection, statement_names[0], 
                                    param_count[0], param_values, NULL, NULL, 0);
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

    res = PQexecPrepared(connection, statement_names[1], param_count[1], 
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
    char fd[2];
    char ip_addr[4];
    unsigned char *ptr = data;
    PGresult *res = NULL;

    if (*ptr == 1) {

        ptr++;
        memcpy(fd, ptr, 2);   

        ptr += 2;
        memcpy(ip_addr, ptr, 4);

        const char *param_values[param_count[0]];
        param_values[0] = fd;
        param_values[1] = ip_addr;
        
        res = PQexecPrepared(connection, statement_names[2], param_count[2], param_values, NULL, NULL, 0);

    }

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(connection));
        return -1;
    }

    PQclear(res);

    return 0;
}


int store_comms_into_database(char *data) 
{
    char msg_id[16];
    char msg_status[1]; 
    unsigned char *ptr = data;
    PGresult* res = NULL;

    if (*ptr == 1) {

        ptr++;
        memcpy(msg_id, ptr, 16);   

        ptr += 16;
        memcpy(msg_status, ptr, 1);

        const char *param_values[param_count[3]];
        param_values[0] = msg_id;
        param_values[1] = msg_status;
        
        res = PQexecPrepared(connection, statement_names[3], param_count[3], 
                                        param_values, NULL, NULL, 0);

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

    res = PQexecPrepared(connection, statement_names[4], param_count[4],
                                    NULL, NULL, NULL, 0);

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

    res = PQexecPrepared(connection, statement_names[5], param_count[5], NULL, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("retriving failed: %s\n", PQerrorMessage(connection));
        return -1;
    }    

    row_count = PQntuples(res);
    if (row_count > 0) {
    
        memcpy(data, PQgetvalue(res, 0, 0), PQgetlength(res, 0, 0));
        PQclear(res);
    
        // update this row from database
        memcpy(cid, data, 4);             
        
        const char *param_values[param_count[6]];
        param_values[0] = cid;        
        
        res = PQexecPrepared(connection, statement_names[6], param_count[6], param_values, NULL, NULL, 0);

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