#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> // contains important fucntionality and api used to create sockets
#include <sys/types.h>  // contains various types required to create a socket   
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h> // contains structures to store address information
#include "sender.h"
#include "partition.h"
#include "shared_memory.h"
#include <string.h>
#include <unistd.h>
#include <syslog.h>

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
    server_address.sin_addr.s_addr = ip_address; 

    /*
        function to connect to remote machine
        first paratmeter is our socket
        second paratmeter is address structure, cast it to a different structure 
        third parameter is size of address structure

        connect returns a integer that will indicate wether connection was succesfull or not
    */
    connection_status = connect(network_socket, (struct sockaddr *)&server_address, sizeof(server_address));
    if (connection_status == -1) {
        return -1;
    }

    syslog(LOG_NOTICE,"Error connecting to server");
    return network_socket;
}


int evaluate_and_take_action(char *data)
{
    int msg_status = -1;
    unsigned char *ptr = data;
    unsigned int port_number = -1;
    unsigned int ipaddress = -1;
    int connection_socket = -1;
    char cid[17];

    if (*ptr == 1) {
        // ippaddres
        // in this case create a connection
        ptr++;
        memcpy(cid, ptr, 16);
        
        ptr += 16;
        port_number = atoi(ptr);
       
        ptr += 4;
        ipaddress = atoi(ptr);
       
        connection_socket = create_connection(port_number, ipaddress);
        
        // communicate to database
        send_message(cid, connection_socket);

    }
    else if (*ptr == 2) {
        
        ptr++;
        memcpy(cid, ptr, 16);
        
        ptr += 16; //internal cid
        connection_socket = atoi(ptr);
        close(network_sokcet);
    }
    else
        return -1;
    
    return 0;
} 


int read_message(char *data) 
{
    int subblock_position = -1;
    char *blkptr = NULL;
    
    sem_wait(smlks.sem_lock_comms);         
    subblock_position = get_subblock(dblks.comms_block, 1, 1);
    
    if (subblock_position >= 0) {

        blkptr = dblks.comms_block + (TOTAL_PARTITIONS/8) + subblock_position * CPARTITION_SIZE;
        memset(data, 0, CPARTITION_SIZE);
        
        memcpy(data, blkptr, CPARTITION_SIZE);
        evaluate_and_take_action(data);
        // if returns -1 then handle appropriately

        blkptr = NULL;
        toggle_bit(subblock_position, dblks.comms_block, 1);
    
    }

    sem_post(smlks.sem_lock_comms);
}


int get_data_from_processor(int *fd, char *data, int *data_size)
{
    int subblock_position = -1;
    char *blkptr = NULL;
    
    sem_wait(smlks.sem_lock_datas);         
    subblock_position = get_subblock(dblks.datas_block , 1, 3);
    
    if(subblock_position >= 0) {

        blkptr = dblks.datas_block + (TOTAL_PARTITIONS/8) + subblock_position * DPARTITION_SIZE;
        memset(data, 0, DPARTITION_SIZE);

        *fd = atoi(blkptr);

        blkptr += 4;
        *data_size = atoi(blkptr);

        blkptr += 4;
        memcpy(data, blkptr, *data_size);
        
        memset(blkptr, 0, DPARTITION_SIZE);
        blkptr = NULL;
        toggle_bit(subblock_position, dblks.datas_block, 3);
    
    }

    sem_post(smlks.sem_lock_datas);

    return subblock_position;
}


int send_data_over_network(unsigned int socketid, char *data, int data_size) 
{
    int shortRetval = -1;  
    shortRetval = send(socketid, data, data_size, 0);
    return shortRetval;
}


int send_message(char *cid, int data) 
{
    int subblock_position = -1;
    char *blkptr = NULL;
    
    sem_wait(smlks.sem_lock_comms);         
    subblock_position = get_subblock(dblks.comms_block, 0, 2);
    
    if(subblock_position >= 0) {

        blkptr = dblks.comms_block + (TOTAL_PARTITIONS/8) + subblock_position * CPARTITION_SIZE;
        memset(blkptr, 0, CPARTITION_SIZE);
        memcpy(blkptr, cid, 16);
        blkptr += 16;
        memcpy(blkptr, &data, sizeof(int));
        toggle_bit(subblock_position, dblks.comms_block, 2);
    
    }

    sem_post(smlks.sem_lock_comms);
}


int run_sender() 
{
    int fd = 0;
    char data[CPARTITION_SIZE];
    int data_size;
    int status = 0;

    while (sender_status) {

        read_message(data);
        if (get_data_from_processor(&fd, data, &data_size)) {
            
            status = send_data_over_network(fd, data, data_size);
            send_message();
            
        }
    }
}


int main(void) 
{
 
    smlks.sem_lock_datas = sem_open(SEM_LOCK_DATAS, O_CREAT, 0777, 1);
    smlks.sem_lock_comms = sem_open(SEM_LOCK_COMMS, O_CREAT, 0777, 1);

    if (smlks.sem_lock_datas == SEM_FAILED || smlks.sem_lock_comms == SEM_FAILED)
        return -1;
 
    dblks.datas_block = attach_memory_block(FILENAME, DATA_BLOCK_SIZE, PROJECT_ID_DATAS);
    dblks.comms_block = attach_memory_block(FILENAME, COMM_BLOCK_SIZE, PROJECT_ID_COMMS);

    if (!(dblks.datas_block && dblks.comms_block)) 
        return -1; 
    
 
    run_sender();
    
    sem_close(smlks.sem_lock_datas);
    sem_close(smlks.sem_lock_comms);
    detach_memory_block(dblks.datas_block);
    detach_memory_block(dblks.comms_block);
    
    return 0;
}