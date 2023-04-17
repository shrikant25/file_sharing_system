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
#include "sender.h"
#include "partition.h"
#include "shared_memory.h"


datablocks dblks;
semlocks smlks;
int sender_status = 1;

int create_connection(unsigned short int port_number, unsigned int ip_address) 
{    
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
    if (connection_status == -1) {
        return -1;
    }

    syslog(LOG_NOTICE,"Error connecting to server");
    return connection_status;
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
    char data[CPARTITION_SIZE];
    open_connection *opncn; 
    close_connection *clscn;
    message_status msgsts;
    send_message sndmsg;
    connection_status cncsts;
   
    while (sender_status) {

        
        if (get_message_from_processor(data) != -1) {
            if (*(int *)data == 1) {
                
                opncn = (open_connection *)data;
                memset(&cncsts, 0, sizeof(connection_status));
                cncsts.type = 3;
                cncsts.fd = create_connection(opncn->port, opncn->ipaddress);
                cncsts.ipaddress = opncn->ipaddress;
                
                send_message_to_processor(3, (void *)&cncsts);

            }
            else if(*(int *)data == 2) {

                clscn = (close_connection *)data;
                close(clscn->fd);
            }
        }

        
        if (get_data_from_processor(&sndmsg)!= -1) {

            memset(&msgsts, 0, sizeof(message_status));
            
            msgsts.type = 4;
            msgsts.status = send(sndmsg.fd, sndmsg.data, strlen(sndmsg.data), 0);
            strncpy(msgsts.uuid, sndmsg.uuid, strlen(sndmsg.uuid));

            send_message_to_processor(4, (void *)&msgsts);
        }
    }
}


int main(void) 
{
 
    smlks.sem_lock_datas = sem_open(SEM_LOCK_DATAS, O_CREAT, 0777, 1);
    smlks.sem_lock_comms = sem_open(SEM_LOCK_COMMS, O_CREAT, 0777, 1);

    if (smlks.sem_lock_datas == SEM_FAILED || smlks.sem_lock_comms == SEM_FAILED)
        return -1;
 
    dblks.datas_block = attach_memory_block(FILENAME_S, DATA_BLOCK_SIZE, PROJECT_ID_DATAS);
    dblks.comms_block = attach_memory_block(FILENAME_S, COMM_BLOCK_SIZE, PROJECT_ID_COMMS);

    if (!(dblks.datas_block && dblks.comms_block)) 
        return -1; 

    run_sender();
    
    sem_close(smlks.sem_lock_datas);
    sem_close(smlks.sem_lock_comms);
    detach_memory_block(dblks.datas_block);
    detach_memory_block(dblks.comms_block);
    
    return 0;
}