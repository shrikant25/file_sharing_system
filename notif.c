#include <stdio.h> 
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <libpq-fe.h>
#include <sys/epoll.h>
#include "shared_memory.h"

int dbsocket;
int epoll_fd;
char error[100];
char noti_channel[30];
struct epoll_event event;
PGconn *connection;
semlocks sem_lock_sig;


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

    if (read(conffd, buf, sizeof(buf)) > 0) {
        sscanf(buf,"SEM_LOCK_SIG=%s\nUSERNAME=%s\nDBNAME=%s\nNOTI_CHANNEL=%s",  sem_lock_sig.key, username, dbname, noti_channel);
    }
    else {
        syslog(LOG_NOTICE, "failed to read configuration file");
        return -1;
    }

    sprintf(noti_command, "LISTEN %s", noti_channel);
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

    dbsocket = PQsocket(connection);
    if (dbsocket == -1) {
        sprintf(error, "noti channel %s, error in PQsocket creating a connection to db %s", noti_channel, PQerrorMessage(connection));
        store_log(error);
        PQfinish(connection);
        return -1;
    }
    PQsetnonblocking(connection, 1);

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        sprintf(error, "epoll_create1 failed in notifier listening on %s", noti_channel);
        store_log(error);
        PQfinish(connection);
        return -1;
    }

    event.events = EPOLLIN;
    event.data.fd = dbsocket;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, dbsocket, &event) == -1) {
        sprintf(error, "epollctl failed in notifier listening on %s", noti_channel);
        store_log(error);
        PQfinish(connection);
        close(epoll_fd);
        return -1;
    }

    PQclear(res);
    res = PQexec(connection, noti_command);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        sprintf(error, "noti channel %s, LISTEN command failed: %s", noti_channel, PQerrorMessage(connection));
        store_log(error);
        PQclear(res);
        close(epoll_fd);
        PQfinish(connection);
        return -1;
    }
    PQclear(res);
    
    if ((sem_lock_sig.var = sem_open(sem_lock_sig.key, O_CREAT, 0777, 0)) == SEM_FAILED) {
        sprintf(error, "noti channel %s error creating semphore %s: %s", noti_channel, sem_lock_sig.key, PQerrorMessage(connection));
        store_log(error);
        close(epoll_fd);
        PQfinish(connection);
        return -1;
    }

    return 0;
}


int main(int argc, char *argv[]) 
{
    PGresult *res;
    PGnotify *notify;
    int num_events;

    if (argc != 2) {
        syslog(LOG_NOTICE, "invlaid arguments");
    }
    
    if(initnotif(argv[1]) == -1) {return -1;}
    
    while (1) {

        num_events = epoll_wait(epoll_fd, &event, 1, -1);
        if (num_events == -1) {
            memset(error, 0, sizeof(error));
            sprintf(error, "epoll wait failed for notifier listening on channel %s", noti_channel);
            store_log(error);
            break;
        }

        if (event.data.fd == dbsocket) {
        
            if (PQconsumeInput(connection) == 0) {
                memset(error, 0, sizeof(error));
                sprintf(error, "noti channel %s Failed to consume input: %s", noti_channel, PQerrorMessage(connection));
                store_log(error);
                break;
            }
            else {
                while ((notify = PQnotifies(connection)) != NULL) {
                    sem_post(sem_lock_sig.var);
                    PQfreemem(notify);
                }
            }
        }
        else {
            memset(error, 0, sizeof(error));
            sprintf(error, "unkown event occured on epoll of notifier listening on channel %s", noti_channel);
            store_log(error);
            break;
        }    
    }
    
    store_log("closing notif");
    sem_close(sem_lock_sig.var);
    PQfinish(connection);
    close(epoll_fd);

    return 0;
}
