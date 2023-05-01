#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> // contains important fucntionality and api used to create sockets
#include <sys/types.h>  // contains various types required to create a socket   
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h> // contains structures to store address information
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <syslog.h>
#include <libpq-fe.h>
#include <errno.h>
#include "sender.h"
#include "partition.h"
#include "shared_memory.h"

semlocks sem_lock_datas;
semlocks sem_lock_comms;
semlocks sem_lock_sigs;
semlocks sem_lock_sigps;
datablocks datas_block;
datablocks comms_block;
PGconn *connection;

int create_connection(unsigned short int port_number, unsigned int ip_address) 
{    
    char error[100];
    int network_socket; // to hold socket file descriptor
    int connection_status; // to show wether a connection was established or not
    struct sockaddr_in server_address; // create a address structure to store the address of remote connection
    /* 
        create a socket
       first parameter is domain of the socket = AF_INET represents that it is an internet socket
       second parameter is type of socket = SOCK_STREAM represents that it is a TCP sockets
       third parametets specifies that protocol that is being used, since we want to use default i.e tcp
       hence it will be set to 0 
    */
    network_socket = socket(AF_INET, SOCK_STREAM, 0);
    
    // address of family 
    server_address.sin_family = AF_INET;    
    
    /*  
        port number
        htons converts the unsigned short from host byte order to network byte order
        host byte order is mainly big-endian, 
        while local machine may have little-endian byte order 
    */
    server_address.sin_port = htons(port_number);  

    /*  
        actual server address i.e Ip address 
        sin_addr is a structure that contains the field to hold the address
        
        in order to connect to local machine address can be 0000 or constant INADDR_ANY
    */
    server_address.sin_addr.s_addr = htonl(ip_address); 

    /*
        function to connect to remote machine
        first paratmeter is our socket
        second paratmeter is address structure, cast it to a different structure 
        third parameter is size of address structure

        connect returns a integer that will indicate wether connection was succesfull or not
    */
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(network_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    connection_status = connect(network_socket, (struct sockaddr *)&server_address, sizeof(server_address));
    return (connection_status == -1 ? connection_status : network_socket);
      
}


int get_message_from_processor(char *data) 
{
    int subblock_position = -1;
    char *blkptr;

    sem_wait(sem_lock_comms.var);         
    subblock_position = get_subblock(comms_block.var, 1, 1);
    
    if (subblock_position >= 0) {

        blkptr = comms_block.var + (TOTAL_PARTITIONS/8) + subblock_position * CPARTITION_SIZE;
        memset(data, 0, CPARTITION_SIZE);
        memcpy(data, blkptr, CPARTITION_SIZE);
        toggle_bit(subblock_position, comms_block.var, 1);

    }
    sem_post(sem_lock_comms.var);
    return subblock_position;       
}


int get_data_from_processor(send_message *sndmsg)
{
    int subblock_position = -1;
    char *blkptr;
    
    sem_wait(sem_lock_datas.var);         
    subblock_position = get_subblock(datas_block.var, 1, 3);
    
    if (subblock_position >= 0) {

        blkptr = datas_block.var + (TOTAL_PARTITIONS/8) + subblock_position * DPARTITION_SIZE;
        memcpy(sndmsg, blkptr, sizeof(send_message));
        toggle_bit(subblock_position, datas_block.var, 3);
    
    }

    sem_post(sem_lock_datas.var);

    return subblock_position;
}


int send_message_to_processor(int type, void *msg) 
{
    int subblock_position = -1;
    char *blkptr = NULL;

    sem_wait(sem_lock_comms.var);         
    subblock_position = get_subblock(comms_block.var, 0, 2);
    
    if(subblock_position >= 0) {

        blkptr = comms_block.var + (TOTAL_PARTITIONS/8) + subblock_position * CPARTITION_SIZE;
        memset(blkptr, 0, CPARTITION_SIZE);

        if (type == 3) {
            memcpy(blkptr, (connection_status *)msg, sizeof(open_connection));
        }
        else if(type == 4) {
            memcpy(blkptr, (message_status *)msg, sizeof(message_status));
        }
        toggle_bit(subblock_position, comms_block.var, 2);
        sem_post(sem_lock_sigps.var);
    }

    sem_post(sem_lock_comms.var);
    return subblock_position;
}


int run_sender() 
{
    int data_sent;
    int total_data_sent;
    char error[100];
    char data[CPARTITION_SIZE];
    open_connection *opncn; 
    close_connection *clscn;
    message_status msgsts;
    send_message sndmsg;
    connection_status cncsts;
   
    while (1) {

        sem_wait(sem_lock_sigs.var);
        if (get_message_from_processor(data) != -1) {
            if (*(unsigned char *)data == 1) {
                
                opncn = (open_connection *)data;
                memset(&cncsts, 0, sizeof(connection_status));
                cncsts.type = 3;
                cncsts.fd = create_connection(opncn->port, opncn->ipaddress);
                cncsts.ipaddress = opncn->ipaddress;
                
                send_message_to_processor(3, (void *)&cncsts);

            }
            else if(*(unsigned char *)data == 2) {

                clscn = (close_connection *)data;
                close(clscn->fd);
            }
        }

        
        if (get_data_from_processor(&sndmsg)!= -1) {

            memset(&msgsts, 0, sizeof(message_status));
            data_sent = 0;
            total_data_sent = 0;
            
            
            do {

                data_sent = send(sndmsg.fd, sndmsg.data+total_data_sent, sizeof(sndmsg.data), 0);
                total_data_sent += data_sent;
                
            }while (total_data_sent < sizeof(sndmsg.data) && data_sent != 0);
            
            msgsts.status = total_data_sent < strlen(sndmsg.data) ? 0 : 1;
            memset(error, 0, sizeof(error));
            sprintf(error, "total bytes sent %s\n", total_data_sent);
            store_log(error);

            msgsts.type = 4;
            strncpy(msgsts.uuid, sndmsg.uuid, strlen(sndmsg.uuid));
            send_message_to_processor(4, (void *)&msgsts);
        }
    }
}


void store_log(char *logtext) {

    PGresult *res = NULL;
    char log[100];
    memset(log, 0, sizeof(log));
    strncpy(log, logtext, strlen(logtext));

    const char *const param_values[] = {log};
    const int paramLengths[] = {sizeof(log)};
    const int paramFormats[] = {0};
    int resultFormat = 0;
    
    res = PQexecPrepared(connection, "s_storelog", 1, param_values, paramLengths, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        syslog(LOG_NOTICE, "logging failed %s , log %s\n", PQerrorMessage(connection), log);
    }

    PQclear(res);
}


int connect_to_database(char *conninfo) 
{   
    connection = PQconnectdb(conninfo);

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
}


int main(int argc, char *argv[]) 
{
    int status = -1;
    int conffd = -1;
    char buf[500];
    char db_conn_command[100];
    char username[30];
    char dbname[30];

    if (argc != 2) {
        syslog(LOG_NOTICE,"invalid arguments");
        return -1;
    }

    if ((conffd = open(argv[1], O_RDONLY)) == -1) {
        syslog(LOG_NOTICE, "failed to open configuration file");
        return -1;
    }

    if (read(conffd, buf, sizeof(buf)) > 0) {
    
        sscanf(buf, "SEM_LOCK_DATAS=%s\nSEM_LOCK_COMMS=%s\nSEM_LOCK_SIG_S=%s\nSEM_LOCK_SIG_PS=%s\nPROJECT_ID_DATAS=%d\nPROJECT_ID_COMMS=%d\nUSERNAME=%s\nDBNAME=%s", sem_lock_datas.key, sem_lock_comms.key, sem_lock_sigs.key, sem_lock_sigps.key, &datas_block.key, &comms_block.key, username, dbname);
    }
    else {
        syslog(LOG_NOTICE, "failed to read configuration file");
        return -1;
    }
    
    
    //destroy unnecessary data;
    memset(buf, 0, sizeof(buf));

    close(conffd);

    sprintf(db_conn_command, "user=%s dbname=%s", username, dbname);
    if (connect_to_database(db_conn_command) == -1) { return -1; }
    if (prepare_statements() == -1) { return -1; }   

    sem_lock_datas.var = sem_open(sem_lock_datas.key, O_CREAT, 0777, 1);
    sem_lock_comms.var = sem_open(sem_lock_comms.key, O_CREAT, 0777, 1);
    sem_lock_sigs.var = sem_open(sem_lock_sigs.key, O_CREAT, 0777, 1);
    sem_lock_sigps.var = sem_open(sem_lock_sigps.key, O_CREAT, 0777, 1);

    if (sem_lock_sigs.var == SEM_FAILED || sem_lock_sigps.var == SEM_FAILED || sem_lock_datas.var == SEM_FAILED || sem_lock_comms.var == SEM_FAILED) {
        store_log("not able to initialize locks");
        return -1;
    }
 
    datas_block.var = attach_memory_block(FILENAME_S, DATA_BLOCK_SIZE, datas_block.key);
    comms_block.var = attach_memory_block(FILENAME_S, COMM_BLOCK_SIZE, comms_block.key);

    if (!(datas_block.var && comms_block.var)) { 
        store_log("not able to attach shared memory");
        return -1; 
    }

    run_sender();

    PQfinish(connection);  

    sem_close(sem_lock_datas.var);
    sem_close(sem_lock_comms.var);
    sem_close(sem_lock_sigs.var);
    sem_close(sem_lock_sigps.var);
    
    detach_memory_block(datas_block.var);
    detach_memory_block(comms_block.var);
    
    return 0;
}