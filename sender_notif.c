#include <stdio.h> 
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <libpq-fe.h>
#include "processor_r.h"
#include "shared_memory.h"

PGconn *connection;
semlocks sem_lock_sigps;
char error[100];

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

int init_semaphore() {
    
    int conffd = -1;
    int temp_int;
    char temp_char[20];
    char buf[500];

    if ((conffd = open("./keys.conf", O_RDONLY)) == -1) {
        store_log("failed to open configuration file");
        return -1;
    }

    if (read(conffd, buf, sizeof(buf)) > 0) {
    
        sscanf(buf, "SEM_LOCK_DATAR=%s\nSEM_LOCK_COMMR=%s\nSEM_LOCK_DATAS=%s\nSEM_LOCK_COMMS=%s\n\
                    SEM_LOCK_SIG_R=%s\nSEM_LOCK_SIG_S=%s\nSEM_LOCK_SIG_PS=%s\n",\
                    temp_char, temp_char, temp_char, temp_char, temp_char, temp_char, sem_lock_sigps.key);
    }
    else {
        store_log("failed to read configuration file");
        return -1;
    }
    
    //destroy unnecessary data;
    memset(buf, 0, sizeof(buf));
    memset(temp_char, 0, sizeof(temp_char));
    temp_int = -1;
    
    if ((sem_lock_sigps.var = sem_open(sem_lock_sigps.key, O_CREAT, 0777, 0)) == SEM_FAILED) {
        memset(error, 0, sizeof(error));
        sprintf(error, "error creating semphore sigps: %s", PQerrorMessage(connection));
        store_log(error);
        return -1;
    }
    return 0;
}




int main(void) {

    PGresult *res;
    PGnotify *notify;

    connection = PQconnectdb("user = shrikant dbname = shrikant ");
    if (PQstatus(connection) != CONNECTION_OK) {
        syslog(LOG_NOTICE, "failed to create connection to database %s", PQerrorMessage(connection));
        return -1;
    }

    res = PQprepare(connection, "store_log", "INSERT INTO logs (log) VALUES ($1);", 1, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        syslog(LOG_NOTICE, "Preparation of statement failed: %s\n", PQerrorMessage(connection));
        PQclear(res);
        PQfinish(connection);
        return -1;
    }
    PQclear(res);

    res = PQexec(connection, "LISTEN senders_channel");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        memset(error, 0, sizeof(error));
        sprintf(error, "LISTEN command failed: %s", PQerrorMessage(connection));
        store_log(error);
        PQclear(res);
        PQfinish(connection);
        return -1;
    }
    PQclear(res);

    if(init_semaphore() == -1) {return -1;}

    while (1) {

        if (PQconsumeInput(connection) == 0) {
            memset(error, 0, sizeof(error));
            sprintf(error, "Failed to consume input: %s", PQerrorMessage(connection));
            store_log(error);
        }
        else{
            while ((notify = PQnotifies(connection)) != NULL) {
                sem_post(sem_lock_sigps.var);
                PQfreemem(notify);
            }
        }
    
    }
    
    sem_close(sem_lock_sigps.var);
    PQfinish(connection);

    return 0;
}
