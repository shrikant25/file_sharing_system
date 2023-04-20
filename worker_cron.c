
#include <libpq-fe.h>
#include <libpq/libpq-fs.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <syslog.h>
#include <string.h>

PGconn *connection;

int connect_to_database() 
{   
    connection = PQconnectdb("user = shrikant dbname = shrikant");
    if (PQstatus(connection) == CONNECTION_BAD) {
        syslog(LOG_NOTICE, "Connection to database failed: %s\n", PQerrorMessage(connection));
        return -1;
    }

    return 0;
}


int prepare_statements() 
{    
    int i;
    PGresult *res = NULL;
    
    res = PQprepare(connection, "storelog", "INSERT INTO logs (log) VALUES ($1);", 1, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        syslog(LOG_NOTICE, "Preparation of statement failed: %s\n", PQerrorMessage(connection));
        return -1;
    }

    PQclear(res);
    

    return 0;
}


void store_log(char *logtext) {

    PGresult *res = NULL;
    char log[100];
    strncpy(log, logtext, strlen(logtext));

    const char *const param_values[] = {log};
    const int paramLengths[] = {sizeof(log)};
    const int paramFormats[] = {0};
    int resultFormat = 0;
    
    res = PQexecPrepared(connection, "storelog", 1, param_values, paramLengths, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        syslog(LOG_NOTICE, "logging failed %s , log %s\n", PQerrorMessage(connection), log);
    }

    PQclear(res);
}



void become_daemon()
{
 
    pid_t pid;

    // make port of parent process
    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);

    // kill parent
    if (pid > 0)
        exit(EXIT_SUCCESS);

    // if >= 0 means child process has become session leader
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Catch, ignore and handle signals */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    //fork again
    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);

    // again parent dies
    if (pid > 0)
        exit(EXIT_SUCCESS);

    // set file premissions
    umask(0);

}


int main()
{
    become_daemon();

    PGresult *res = NULL;
    
    if (connect_to_database() == -1) { return -1;}
    if (prepare_statements() == -1) {return -1;}

    syslog (LOG_NOTICE, "process started.");
    
    while (1) {
    
        store_log("hello");
        PQclear(res);
        sleep(1);
    }

    syslog(LOG_NOTICE, "process finished");
    return EXIT_SUCCESS;
}

//gcc daemon.c -o daemon  -I/usr/include/postgresql -lpq -std=c99


// create or replace function launch_worker() 
// returns void 
// as 
// $$ 
// begin 
//  Insert into logs(log) values('hurra');
// end; 
// $$ 
// LANGUAGE 'plpgsql';
