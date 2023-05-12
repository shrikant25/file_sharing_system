#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h> // contains important fucntionality and api used to create sockets
#include <sys/types.h>  // contains various types required to create a socket   
#include <netinet/in.h> // contains structures to store address information
#include <sys/types.h>
#include <libpq-fe.h>
#include "initial_sender.h"

server_info s_info;
PGconn *connection;
char error[100];

int read_data_from_database (server_info *servinfo) {

    int status = -1;
    PGresult *res = PQexecPrepared(connection, dbs[1].statement_name, dbs[1].param_count, NULL, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) <= 0) {
        memset(error, 0, sizeof(error));
        sprintf(error, "retriving data for intial sender failed %s", PQerrorMessage(connection));
        store_log(error);
    }    
    else {
        servinfo->port = atoi(PQgetvalue(res, 0, 0));
        servinfo->ipaddress = atoi(PQgetvalue(res, 0, 1)); 
        strncpy(servinfo->data, PQgetvalue(res, 0, 2), MESSAGE_SIZE);
        status = 0;
    }

    PQclear(res);
    return status;
}


void run_server () {
    
    int hSocket, send_size, idx, fd;
    unsigned int ipaddress;
    struct sockaddr_in server;
    server_info servinfo;
    PGnotify *notify;
    
    while (1) {
        
        if (PQconsumeInput(connection) == 0) {
            memset(error, 0, sizeof(error));
            sprintf(error, "Failed to consume input: %s", PQerrorMessage(connection));
            store_log(error);
        }
        else{

            while ((notify = PQnotifies(connection)) != NULL) {

                memset(&servinfo, 0, sizeof(servinfo)); 
                if (read_data_from_database(&servinfo) != -1) {

                    servinfo.servsoc_fd = socket(AF_INET, SOCK_STREAM, 0);
                    if (servinfo.servsoc_fd == -1) {
                        memset(error, 0, sizeof(error));
                        sprintf(error, "initial sender failed to create client socket %s", strerror(errno));
                        store_log(error);
                        return;
                    }
                    else {
                    
                        server.sin_addr.s_addr = servinfo.ipaddress;
                        server.sin_family = AF_INET;
                        server.sin_port = htons(servinfo.port);
                        
                        if ((connect(servinfo.servsoc_fd, (struct sockaddr *)&server, sizeof(struct sockaddr_in))) == -1) {
                            memset(error, 0, sizeof(error));
                            sprintf(error, "failed to connect form connection with remote host %s", strerror(errno));
                            store_log(error);
                        }
                        else {
                            send_size = send(servinfo.servsoc_fd, servinfo.data, MESSAGE_SIZE, 0);
                            if (send_size <= 0) {
                                memset(error, 0, sizeof(error));
                                sprintf(error, "failed to send data %s", strerror(errno));
                                store_log(error);
                            }
                        }

                        close(servinfo.servsoc_fd);
                    }
                }
            }
        }
    }
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
    
    res = PQexecPrepared(connection, dbs[0].statement_name, dbs[0].param_count, param_values, paramLengths, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        syslog(LOG_NOTICE, "logging failed %s , log %s\n", PQerrorMessage(connection), log);
    }

    PQclear(res);
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
    char buf[100];
    char db_conn_command[100];
    char username[30];
    char dbname[30];
    char noti_channel[30];
    char noti_command[100];
    PGresult *res;

    if (argc != 2) {
        syslog(LOG_NOTICE,"invalid arguments");
        return -1;
    }
   
    if ((conffd = open(argv[1], O_RDONLY)) == -1) {
        syslog(LOG_NOTICE, "failed to open configuration file");
        return -1;
    }

    if (read(conffd, buf, sizeof(buf)) > 0) {
        sscanf(buf, "USERNAME=%s\nDBNAME=%s\nPORT=%d\nIPADDRESS=%lu\nNOTI_CHANNEL=%s", username, dbname, &s_info.port, &s_info.ipaddress, noti_channel);
    }
    else {
        syslog(LOG_NOTICE, "failed to read configuration file");
        return -1;
    }

    close(conffd);

    sprintf(noti_command, "LISTEN %s", noti_channel);
    sprintf(db_conn_command, "user=%s dbname=%s", username, dbname);

    if (connect_to_database(db_conn_command) == -1) { return -1; }
    if (prepare_statements() == -1) { return -1; }     

    res = PQexec(connection, noti_command);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        memset(error, 0, sizeof(error));
        sprintf(error, "LISTEN command failed: %s", PQerrorMessage(connection));
        store_log(error);
        PQclear(res);
        PQfinish(connection);
        return -1;
    }
    PQclear(res); 

    run_server();
    PQfinish(connection);

    return 0;
    
    exit(0);
}
