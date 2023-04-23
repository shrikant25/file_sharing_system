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


datablocks dblks;
semlocks smlks;
int sender_status = 1;
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

    sem_wait(smlks.sem_lock_comms);         
    subblock_position = get_subblock(dblks.comms_block, 1, 1);
    
    if (subblock_position >= 0) {

        blkptr = dblks.comms_block + (TOTAL_PARTITIONS/8) + subblock_position * CPARTITION_SIZE;
        memset(data, 0, CPARTITION_SIZE);
        memcpy(data, blkptr, CPARTITION_SIZE);
        toggle_bit(subblock_position, dblks.comms_block, 1);

    }
    sem_post(smlks.sem_lock_comms);
    return subblock_position;       
}


int get_data_from_processor(send_message *sndmsg)
{
    int subblock_position = -1;
    char *blkptr;
    
    sem_wait(smlks.sem_lock_datas);         
    subblock_position = get_subblock(dblks.datas_block, 1, 3);
    
    if (subblock_position >= 0) {

        blkptr = dblks.datas_block + (TOTAL_PARTITIONS/8) + subblock_position * DPARTITION_SIZE;
        memcpy(sndmsg, blkptr, sizeof(send_message));
        toggle_bit(subblock_position, dblks.datas_block, 3);
    
    }

    sem_post(smlks.sem_lock_datas);

    return subblock_position;
}


int send_message_to_processor(int type, void *msg) 
{
    int subblock_position = -1;
    char *blkptr = NULL;

    sem_wait(smlks.sem_lock_comms);         
    subblock_position = get_subblock(dblks.comms_block, 0, 2);
    
    if(subblock_position >= 0) {

        blkptr = dblks.comms_block + (TOTAL_PARTITIONS/8) + subblock_position * CPARTITION_SIZE;
        memset(blkptr, 0, CPARTITION_SIZE);

        if (type == 3) {
            memcpy(blkptr, (connection_status *)msg, sizeof(open_connection));
        }
        else if(type == 4) {
            memcpy(blkptr, (message_status *)msg, sizeof(message_status));
        }
        toggle_bit(subblock_position, dblks.comms_block, 2);
    
    }

    sem_post(smlks.sem_lock_comms);
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
   
    while (sender_status) {

        
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

                data_sent = send(sndmsg.fd, sndmsg.data, strlen(sndmsg.data), 0);
                total_data_sent += data_sent;
                
            }while (total_data_sent < strlen(sndmsg.data) && data_sent != 0);
            
            msgsts.status = total_data_sent < strlen(sndmsg.data) ? 0 : 1;

            msgsts.type = 4;
            strncpy(msgsts.uuid, sndmsg.uuid, strlen(sndmsg.uuid));
            send_message_to_processor(4, (void *)&msgsts);
        }
    }
}


void store_log(char *logtext) {

    PGresult *res = NULL;
    char log[100];
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


int connect_to_database() 
{   
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
}

int main(void) 
{
    if (connect_to_database() == -1) {
        return -1;
    }

    if (prepare_statements() == -1) {
        return -1;
    }

    smlks.sem_lock_datas = sem_open(SEM_LOCK_DATAS, O_CREAT, 0777, 1);
    smlks.sem_lock_comms = sem_open(SEM_LOCK_COMMS, O_CREAT, 0777, 1);

    if (smlks.sem_lock_datas == SEM_FAILED || smlks.sem_lock_comms == SEM_FAILED) {
        store_log("not able to initialize locks");
        return -1;
    }
 
    dblks.datas_block = attach_memory_block(FILENAME_S, DATA_BLOCK_SIZE, PROJECT_ID_DATAS);
    dblks.comms_block = attach_memory_block(FILENAME_S, COMM_BLOCK_SIZE, PROJECT_ID_COMMS);

    if (!(dblks.datas_block && dblks.comms_block)) { 
        store_log("not able to attach shared memory");
        return -1; 
    }

    run_sender();

    PQfinish(connection);  
    sem_close(smlks.sem_lock_datas);
    sem_close(smlks.sem_lock_comms);
    detach_memory_block(dblks.datas_block);
    detach_memory_block(dblks.comms_block);
    
    return 0;
}