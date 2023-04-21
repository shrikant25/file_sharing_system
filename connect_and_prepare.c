#include <libpq-fe.h>
#include <syslog.h>
#include "connect_and_prepare.h"

int connect_to_database(PGconn *connection) 
{   
    connection = PQconnectdb("user = shrikant dbname = shrikant");
    if (PQstatus(connection) == CONNECTION_BAD) {
        syslog(LOG_NOTICE, "Connection to database failed: %s\n", PQerrorMessage(connection));
    }

    return 0;
}


int prepare_statements(PGconn *connection, int statement_count, db_statements *dbs) 
{    
    int i;

    for(i = 0; i<statement_count; i++){

        PGresult* res = PQprepare(connection, dbs[i].statement_name, 
                                    dbs[i].statement, dbs[i].param_count, NULL);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            syslog(LOG_NOTICE, "Preparation of statement failed: %s\n", PQerrorMessage(connection));
            return -1;
        }

        PQclear(res);
    }

    return 0;
}