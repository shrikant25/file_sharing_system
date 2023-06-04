#include <time.h>
#include <aio.h>
#include "partition.h"

int epoll_fd;
PGconn *connection;

void storelog(char * fmt, ...);

int initnotif(char *confg_filename) 
{
    
    int conffd = -1;
    char buf[500];
    char username[30];
    char dbname[30];
    char db_conn_command[100];
    char funcname[10];
    char func_command[30];

    PGresult *res = NULL;
    if ((conffd = open(confg_filename, O_RDONLY)) == -1) {
        syslog(LOG_NOTICE, "failed to open configuration file");
        return -1;
    }

    memset(buf, 0, sizeof(buf));
    memset(db_conn_command, 0, sizeof(db_conn_command));
    memset(username, 0, sizeof(username));
    memset(dbname, 0, sizeof(dbname));
    memset(funcname, 0, sizeof(funcname));
    memset(func_command, 0, sizeof(func_command));

    if (read(conffd, buf, sizeof(buf)) > 0) {
        sscanf(buf,"FUNCNAME=%s\nUSERNAME=%s\nDBNAME=%s\n",  funcname, username, dbname);
    }
    else {
        syslog(LOG_NOTICE, "failed to read configuration file");
        return -1;
    }

    sprintf(func_command, "Select %s();", funcname);
    sprintf(db_conn_command, "user=%s dbname=%s", username, dbname);

    connection = PQconnectdb(db_conn_command);
    if (PQstatus(connection) != CONNECTION_OK) {
        syslog(LOG_NOTICE, "failed to create connection to database user %s dbname %s error %s", username, dbname, PQerrorMessage(connection));
        return -1;
    }

    res = PQprepare(connection, "run_jobs", func_command, 0, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        storelog("%s%s", "preparation of statement run jobs failed : ", PQerrorMessage(connection));
        PQclear(res);
        PQfinish(connection);
        return -1;
    }

    PQclear(res);

    return 0;
}

void do_run_jobs() {

    PGresult *res;
    res = PQexecPrepared(connection, "run_jobs", 0, NULL, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK){
        storelog("%s%s", "failed to run jobs function : ", PQerrorMessage(connection));
    }
    PQclear(res);
}


int main(int argc, char *argv[]) 
{
    int num_events = 0;
    int turn = 0;
    PGnotify *notify;

    const struct timespec tm = {
        3, 
        0L
    };

    if (argc != 2) {
        syslog(LOG_NOTICE, "invlaid arguments");
    }
    
    if(initnotif(argv[1]) == -1) {return -1;}
    
    while (1) {
        nanosleep(&tm, NULL);
        do_run_jobs();         
    }
    
    PQfinish(connection);

    return 0;
}
