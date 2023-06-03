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
void storelog(char * fmt, ...);

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
        storelog("%s%s", "noti channel : ", noti_channel, " error in PQsocket creating a connection to db : ", PQerrorMessage(connection));
        PQfinish(connection);
        return -1;
    }
    PQsetnonblocking(connection, 1);

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        storelog("%s%s", "epoll_create1 failed in notifier listening on : ", noti_channel);
        PQfinish(connection);
        return -1;
    }

    event.events = EPOLLIN;
    event.data.fd = dbsocket;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, dbsocket, &event) == -1) {
        storelog("%s%s", "epollctl failed in notifier listening on : ", noti_channel);
        PQfinish(connection);
        close(epoll_fd);
        return -1;
    }

    PQclear(res);
    res = PQexec(connection, noti_command);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        storelog("%s%s%s%s", "noti channel : ", noti_channel, " LISTEN command failed : ", PQerrorMessage(connection));
        PQclear(res);
        close(epoll_fd);
        PQfinish(connection);
        return -1;
    }
    PQclear(res);
    
    if ((sem_lock_sig.var = sem_open(sem_lock_sig.key, O_CREAT, 0777, 0)) == SEM_FAILED) {
        storelog("%s%s%s%s", "noti channel : ", noti_channel,  " error creating semphore : ", sem_lock_sig.key, PQerrorMessage(connection));
        close(epoll_fd);
        PQfinish(connection);
        return -1;
    }

    return 0;
}


int main(int argc, char *argv[]) 
{
    PGnotify *notify;
    int num_events;

    if (argc != 2) {
        syslog(LOG_NOTICE, "invlaid arguments");
    }
    
    if(initnotif(argv[1]) == -1) {return -1;}
    
    while (1) {

        num_events = epoll_wait(epoll_fd, &event, 1, -1);
        if (num_events == -1) {
            storelog("%s%s", "epoll wait failed for notifier listening on channel : ", noti_channel);
            break;
        }

        if (event.data.fd == dbsocket) {
        
            if (PQconsumeInput(connection) == 0) {
                storelog("%s%s%s%s", "noti channel : ", noti_channel ," Failed to consume input : ", PQerrorMessage(connection));
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
            storelog("%s%s", "unkown event occured on epoll of notifier listening on channel : ", noti_channel);
            break;
        }    
    }
    
    sem_close(sem_lock_sig.var);
    PQfinish(connection);
    close(epoll_fd);

    return 0;
}
