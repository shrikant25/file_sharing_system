#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libpq-fe.h>
#include "user_program.h"

PGconn *connection;
char error[100];

int connect_to_database(char *conninfo) 
{   
    connection = PQconnectdb(conninfo);

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


void store_log(char *logtext) 
{

    PGresult *res = NULL;
    char log[100];
    memset(log, 0, sizeof(log));
    strncpy(log, logtext, strlen(logtext));

    const char *const param_values[] = {log};
    const int paramLengths[] = {sizeof(log)};
    const int paramFormats[] = {0};
    int resultFormat = 0;
    
    res = PQexecPrepared(connection, "u_storelog", 1, param_values, paramLengths, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        syslog(LOG_NOTICE, "logging failed %s , log %s\n", PQerrorMessage(connection), log);
    }

    PQclear(res);
}


int main(int argc, char *argv[]) {

    char path[pathsize];    
    char confdata[confdatasize];
    char db_conn_command[db_conn_command_size];
    char username[conn_param_size];
    char dbname[conn_param_size];
    int conffd = -1;
    int choice1 = 0;

    if (argc != 2){
        syslog(LOG_NOTICE, "invalid arguments");
        return -1;
    }

    if((conffd = open(argv[1], O_RDONLY)) == -1) {
        syslog(LOG_NOTICE, "failed to open config file");
        return -1;
    }

    memset(confdata, 0, confdatasize);
    if(read(conffd, confdata, confdatasize) > 0){
        sscanf(confdata, "USERNAME=%s\nDBNAME=%s", username, dbname);
    }
    else {
        syslog(LOG_NOTICE, "failed to read config file");
        return -1;
    }

    close(conffd);

    sprintf(db_conn_command, "user=%s dbname=%s", username, dbname);

    if (connect_to_database(db_conn_command) == -1) { return -1;}
    if (prepare_statements() == -1) { return -1;}

    do {
        printf("Enter 1 to import a file into database\n");
        printf("Enter 2 to send a file\n");
        printf("Enter 3 to export a file from database\n");
        scanf("%d", &choice1);

        memset(path, 0, pathsize);
        printf("Enter file path\n");
        
        if (fgets(path, pathsize, stdin)) {
            
            switch (choice1) {
                case 1: //import_file(path);
                        break;
                case 2: //send_file(path);
                        break;
                case 3: //export_file(path);
                        break;
                default: printf("Invalide choice\n");
                        break;
            };
        }
        else {     
            printf("Invalid Input\n");
        }
    } while(1);

    return 0;
}