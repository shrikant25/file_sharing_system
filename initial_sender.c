#include <sys/socket.h> // contains important fucntionality and api used to create sockets
#include <sys/types.h>  // contains various types required to create a socket   
#include <netinet/in.h> // contains structures to store address information
#include <sys/types.h>
#include <semaphore.h>
#include "initial_sender.h"
#include "partition.h"

PGconn *connection;
semlocks sem_lock_isender;

void rollback() 
{
    PGresult *res = PQexec(connection, "ROLLBACK");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        storelog("%s%s", "ROLLBACK command for transaction failed in initial sender : ", PQerrorMessage(connection));
    }
    PQclear(res);
}


int commit() 
{
    int status = 0;

    PGresult *res = PQexec(connection, "COMMIT");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        storelog("%s%s", "COMMIT for transaction command failed in initial sender : ", PQerrorMessage(connection));  
        rollback();
        status = -1;
    }    

    PQclear(res);
    return status;
}


int read_data_from_database (server_info *servinfo) 
{
    PGresult *res;

    res = PQexec(connection, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        storelog("%s%s", "BEGIN command in transaction failed in initial sender : ", PQerrorMessage(connection));
    }
    else {

        PQclear(res);
        res = PQexecPrepared(connection, dbs[1].statement_name, dbs[1].param_count, NULL, NULL, NULL, 0);
        if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) <= 0) {
            storelog("%s%s", "retriving data for intial sender failed : ", PQerrorMessage(connection));
            PQclear(res);
            rollback();
            return -1;
        }    
        else {

            servinfo->port = atoi(PQgetvalue(res, 0, 0));
            servinfo->ipaddress = atoi((PQgetvalue(res, 0, 1)));
            memcpy(servinfo->uuid, PQgetvalue(res, 0, 2), PQgetlength(res, 0, 2)); 

            const char *const param_values[] = {servinfo->uuid};
            const int param_lengths[] = {strlen(servinfo->uuid)};
            const int param_format[] = {0};
            int result_format = 1;  

            PQclear(res);
            res = PQexecPrepared(connection, dbs[2].statement_name, dbs[2].param_count, param_values, param_lengths, param_format, result_format);
            if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) <= 0) {
                storelog("%s%s%s%s", "failed to get message data : ",  servinfo->uuid,  " error : ", PQerrorMessage(connection));
                PQclear(res);
                rollback();
                return -1;
            }
            else {

                memcpy(servinfo->data, PQgetvalue(res, 0, 0), PQgetlength(res, 0, 0));
                PQclear(res);
                return commit();
            }
                
        }
    }
    return -1;
}


void update_status (char *uuid, int status) {

    PGresult *res;
    char status_param[2];

    sprintf(status_param, "%d", status);
    const char *const param_values[] = {status_param, uuid};
    const int param_lengths[] = {sizeof(status_param), strlen(uuid)};
    const int param_format[] = {0, 0};
    int result_format = 0;  
    
    res = PQexecPrepared(connection, dbs[3].statement_name, dbs[3].param_count, param_values, param_lengths, param_format, result_format);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        storelog("%s%s%s%d%s%s", "failed to update status of message", uuid , " status : ",  status,  " error : ", PQerrorMessage(connection));
    }
    
    PQclear(res);
}


void run_server () 
{
    
    int hSocket, data_sent, total_data_sent, idx, fd;
    unsigned int ipaddress;
    struct sockaddr_in server;
    server_info servinfo;
    
    while (1) {
        
        sem_wait(sem_lock_isender.var);
            
        memset(&servinfo, 0, sizeof(servinfo)); 
        if (read_data_from_database(&servinfo) != -1) {
            
            servinfo.servsoc_fd = socket(AF_INET, SOCK_STREAM, 0);
            
            if (servinfo.servsoc_fd == -1) {
                storelog("%s%s", "initial sender failed to create client socket : ", strerror(errno));
                update_status(servinfo.uuid, -1);
            }
            else {
            
                server.sin_family = AF_INET;
                server.sin_port = htons(servinfo.port);
                server.sin_addr.s_addr = htonl(servinfo.ipaddress);
                
                if ((connect(servinfo.servsoc_fd, (struct sockaddr *)&server, sizeof(struct sockaddr_in))) == -1) {
                    storelog("%s%s", "failed to connect form connection with remote host : ", strerror(errno));
                    update_status(servinfo.uuid, -1);
                }
                else {
                    
                    total_data_sent = 0;
                    data_sent = 0;

                    do {
                        
                        data_sent = send(servinfo.servsoc_fd, servinfo.data+total_data_sent, ISMESSAGE_SIZE, 0);
                        total_data_sent += data_sent;
                        
                    } while (total_data_sent < ISMESSAGE_SIZE && data_sent > 0);
                            
                    if (total_data_sent != ISMESSAGE_SIZE) {
                        
                        update_status(servinfo.uuid, -1);
                    }
                    else {
                        update_status(servinfo.uuid, total_data_sent);
                    }
                }

                close(servinfo.servsoc_fd);
            }
        }
    }
}


int connect_to_database (char *conninfo) 
{   
    connection = PQconnectdb(conninfo);
    if (PQstatus(connection) != CONNECTION_OK) {
        syslog(LOG_NOTICE, "Connection to database failed: %s\n", PQerrorMessage(connection));
        return -1;
    }

    return 0;
}


int prepare_statements () 
{    
    int i, status = 0;

    for (i = 0; i<statement_count; i++) {

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


int main (int argc, char *argv[])
{
    int status = -1;
    int conffd = -1;
    char buf[100];
    char db_conn_command[100];
    char username[30];
    char dbname[30];
    char noti_channel[30];
    char noti_command[100];
    PGresult *res;

    if (argc != 2) {
        syslog(LOG_NOTICE,"invalid arguments");
        return -1;
    }
   
    if ((conffd = open(argv[1], O_RDONLY)) == -1) {
        syslog(LOG_NOTICE, "failed to open configuration file");
        return -1;
    }

    memset(buf, 0, sizeof(buf));
    memset(username, 0, sizeof(username));
    memset(dbname, 0, sizeof(dbname));
    memset(&sem_lock_isender, 0, sizeof(semlocks));

    if (read(conffd, buf, sizeof(buf)) > 0) {
        sscanf(buf, "USERNAME=%s\nDBNAME=%s\nSEM_LOCK_ISENDER=%s", username, dbname, sem_lock_isender.key);
    }
    else {
        syslog(LOG_NOTICE, "failed to read configuration file");
        return -1;
    }

    close(conffd);

    sprintf(db_conn_command, "user=%s dbname=%s", username, dbname);

    if (connect_to_database(db_conn_command) == -1) { return -1; }
    if (prepare_statements() == -1) { return -1; }     

    sem_lock_isender.var = sem_open(sem_lock_isender.key, O_CREAT, 0777, 0);
    
    if (sem_lock_isender.var == SEM_FAILED) {
        storelog("%s", "initial sender failed to intialize locks");
        status = -1;
    }

    run_server();

    sem_close(sem_lock_isender.var); // unlink lock used for communication
    PQfinish(connection);

    return 0;
    
    exit(0);
}
