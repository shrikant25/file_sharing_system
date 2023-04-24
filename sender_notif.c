#include <stdio.h>
#include <semaphore.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <libpq-fe.h>
#include "processor_r.h"
#include "shared_memory.h"

PGconn *connection;

void store_log(char *logtext) 
{

    PGresult *res = NULL;
    char log[100];
    strncpy(log, logtext, strlen(logtext));

    const char *const param_values[] = {log};
    const int paramLengths[] = {sizeof(log)};
    const int paramFormats[] = {0};
    int resultFormat = 0;
    
    res = PQexecPrepared(connection, "store_log", 5, param_values, paramLengths, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        syslog(LOG_NOTICE, "logging failed %s , log %s\n", PQerrorMessage(connection), log);
    }

    PQclear(res);
}


int main(void) {

    PGresult *res;
    PGnotify *notify;
    sem_t *sigps;
    char error[100];

    connection = PQconnectdb("user = shrikant dbname = shrikant ");
    if (PQstatus(connection) != CONNECTION_OK) {
        syslog(LOG_NOTICE, "failed to create connection to database %s", PQerrorMessage(connection));
        return -1;
    }

    res = PQprepare(connection, "store_logs", "INSERT INTO LOGS (log) VALUES ($1)", 1, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        syslog(LOG_NOTICE, "Preparation of statement failed: %s\n", PQerrorMessage(connection));
        PQclear(res);
        PQfinish(connection);
        return -1;
    }
    PQclear(res);

    res = PQexec(connection, "LISTEN mychannel");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        memset(error, 0, sizeof(error));
        sprintf(error, "LISTEN command failed: %s", PQerrorMessage(connection));
        store_log(error);
        PQclear(res);
        PQfinish(connection);
        return -1;
    }
    PQclear(res);

    if ((sigps = sem_open(SEM_LOCK_SIG_PS, O_CREAT, 0777, 0)) == SEM_FAILED) {
        memset(error, 0, sizeof(error));
        sprintf(error, "error creating semphore sigps: %s", PQerrorMessage(connection));
        store_log(error);
        return -1;
    }

    while (1) {

        if (PQconsumeInput(connection) == 0) {
            memset(error, 0, sizeof(error));
            sprintf(error, "Failed to consume input: %s", PQerrorMessage(connection));
            store_log(error);
        }

        while ((notify = PQnotifies(connection)) != NULL) {
            sem_post(sigps);
            PQfreemem(notify);
        }
    
    }
    
    sem_close(sigps);
    PQfinish(connection);

    return 0;
}
