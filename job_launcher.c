#include <stdio.h> 
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <libpq-fe.h>
#include <unistd.h>
#include <time.h>
#include <aio.h>

int epoll_fd;
char error[100];
char noti_channel[30];
PGconn *connection;


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
    
    res = PQexecPrepared(connection, "store_log", 1, param_values, paramLengths, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        syslog(LOG_NOTICE, "logging failed %s , log %s\n", PQerrorMessage(connection), log);
    }

    PQclear(res);
}


int initnotif(char *confg_filename) 
{
    
    int conffd = -1;
    char buf[500];
    char username[30];
    char dbname[30];
    char noti_command[100];
    char db_conn_command[100];
    char funcname[10];
    char func_command[30];

    PGresult *res = NULL;
    if ((conffd = open(confg_filename, O_RDONLY)) == -1) {
        syslog(LOG_NOTICE, "failed to open configuration file");
        return -1;
    }

    memset(error, 0, sizeof(error));
    memset(buf, 0, sizeof(buf));
    memset(noti_channel, 0 , sizeof(noti_channel));
    memset(db_conn_command, 0, sizeof(db_conn_command));
    memset(username, 0, sizeof(username));
    memset(dbname, 0, sizeof(dbname));
    memset(funcname, 0, sizeof(funcname));
    memset(func_command, 0, sizeof(func_command));

    if (read(conffd, buf, sizeof(buf)) > 0) {
        sscanf(buf,"FUNCNAME=%s\nUSERNAME=%s\nDBNAME=%s\nNOTI_CHANNEL=%s",  funcname, username, dbname, noti_channel);
    }
    else {
        syslog(LOG_NOTICE, "failed to read configuration file");
        return -1;
    }

    sprintf(noti_command, "LISTEN %s", noti_channel);
    sprintf(func_command, "Select %s();", funcname);
    sprintf(db_conn_command, "user=%s dbname=%s", username, dbname);

    connection = PQconnectdb(db_conn_command);
    if (PQstatus(connection) != CONNECTION_OK) {
        syslog(LOG_NOTICE, "failed to create connection to database user %s dbname %s noti_channel %s error %s", username, dbname, noti_channel, PQerrorMessage(connection));
        return -1;
    }

    res = PQprepare(connection, "store_log", "INSERT INTO logs (log) VALUES ($1);", 1, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        syslog(LOG_NOTICE, "Preparation of statement failed: %s\n", PQerrorMessage(connection));
        PQclear(res);
        PQfinish(connection);
        return -1;
    }

    res = PQprepare(connection, "run_jobs", func_command, 0, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        sprintf(error, "preparation of statement run jobs failed for notif channel %s error %s",  noti_channel, PQerrorMessage(connection));
        store_log(error);
        PQclear(res);
        PQfinish(connection);
        return -1;
    }

   
    res = PQexec(connection, noti_command);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        sprintf(error, "noti channel %s, LISTEN command failed: %s", noti_channel, PQerrorMessage(connection));
        store_log(error);
        PQclear(res);
        close(epoll_fd);
        PQfinish(connection);
        return -1;
    }
    store_log(noti_command);

    PQclear(res);

    return 0;
}

void do_run_jobs() {

    PGresult *res;
    res = PQexecPrepared(connection, "run_jobs", 0, NULL, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK){
        memset(error, 0, sizeof(error));
        sprintf(error, "failed to run jobs function for noti channel %s, errorr : %s", noti_channel, PQerrorMessage(connection));
        store_log(error);
    }
    PQclear(res);
}


int main(int argc, char *argv[]) 
{
    int num_events = 0;
    int turn = 0;
    PGnotify *notify;

    const struct timespec tm = {
        1, 
        0L
    };

    if (argc != 2) {
        syslog(LOG_NOTICE, "invlaid arguments");
    }
    
    if(initnotif(argv[1]) == -1) {return -1;}
    
    do_run_jobs();

    while (1) {
            
        nanosleep(&tm, NULL);
                        
        if (PQconsumeInput(connection) == 0) {
                memset(error, 0, sizeof(error));
                sprintf(error, "noti channel %s Failed to consume input: %s", noti_channel, PQerrorMessage(connection));
                store_log(error);
                break;
        }
        else {
            while ((notify = PQnotifies(connection)) != NULL) {                        
                do_run_jobs();
                PQfreemem(notify);
            }
        }
    }
    
    store_log("closing job_launcher");
    PQfinish(connection);

    return 0;
}
