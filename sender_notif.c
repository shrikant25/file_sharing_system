#include <stdio.h>
#include <libpq-fe.h>

int main(void) {

    PGconn *connection;
    PGresult *res;
    connection = PQconnectdb("user = shrikant dbname = shrikant ");
    if (PQstatus(connection) != CONNECTION_OK) {
        syslog(LOG_NOTICE, "failed to create connection to database %s", PQerrorMessage(connection));
        return -1;
    }


    PGresult *res = PQexec(conn, "LISTEN mychannel");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "LISTEN command failed: %s", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        exit(1);
    }

    while (1) {
    if (PQconsumeInput(conn) == 0) {
        fprintf(stderr, "Failed to consume input: %s", PQerrorMessage(conn));
        PQfinish(conn);
        exit(1);
    }
    PGnotify *notify;
    while ((notify = PQnotifies(conn)) != NULL) {
        printf("Received notification from channel %s: %s\n", notify->relname, notify->extra);
        PQfreemem(notify);
    }
}




}

connection = PQconnectdb("user = shrikant dbname = shrikant");

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