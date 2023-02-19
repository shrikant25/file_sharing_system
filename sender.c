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

datablocks dablks;
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
        printf("Error connecting to server\n");
        return -1;
    }

    return network_socket;
}


int close_connection(unsigned int network_sokcet) 
{
    close(network_sokcet);
}


static int get_shared_memory()
{
    dablks.datas_block = attach_memory_block(FILENAME, DATA_BLOCK_SIZE, PROJECT_ID_DATAS);
    dablks.comms_block = attach_memory_block(FILENAME, COMM_BLOCK_SIZE, PROJECT_ID_COMMS);

    if (!(dablks.datas_block && dablks.comms_block)) 
        return -1; 
    return 0;
}


int detach_memory()
{
    int status = 0;
    
    status = detach_memory_block(dablks.datas_block);
    status = detach_memory_block(dablks.comms_block);

    return status;
}


int initialize_locks() 
{
    int status = 0;

    smlks.sem_lock_datas = sem_open(SEM_LOCK_DATAS, O_CREAT, 0777, 1);
    smlks.sem_lock_comms = sem_open(SEM_LOCK_COMMS, O_CREAT, 0777, 1);

    if (smlks.sem_lock_datas == SEM_FAILED || smlks.sem_lock_comms == SEM_FAILED)
        status = -1;

    return status;
}


int uninitialize_locks()
{
    sem_close(smlks.sem_lock_datas);
    sem_close(smlks.sem_lock_comms);
}


int evaluate_and_take_action(char *data, int *csstart_pos2)
{
    int msg_status = -1;
    unsigned char *ptr = data;
    unsigned int port_number = -1;
    unsigned int ipaddress = -1;
    int connection_socket = -1;
    char cid[16];

    if (*ptr == 1) {
        // ippaddres
        // in this case create a connection
        ptr++;
        memcpy(cid, ptr, 16);
        
        ptr += 16;
        port_number = *(int *)ptr;
       
        ptr += 4;
        ipaddress = *(int *)ptr;
       
        connection_socket = create_connection(port_number, ipaddress);
        
        // communicate to database
        send_message(cid, connection_socket, csstart_pos2);

    }
    else if (*ptr == 2) {
        
        ptr++;
        memcpy(cid, ptr, 16);
        
        ptr += 16; //internal cid
        connection_socket = *(int *)ptr;
        msg_status = close_connection(connection_socket);
        send_message(cid, msg_status, csstart_pos2);
        
    }
    else{
        return -1;
    }

    return 0;

} 


int read_message(char *data, int *csstart_pos1, int *csstart_pos2) 
{
    int subblock_position = -1;
    char *blkptr = NULL;
    
    sem_wait(smlks.sem_lock_comms);         
    subblock_position = get_subblock(dablks.comms_block , 1, *csstart_pos1);
    
    if(subblock_position >= 0) {

        *csstart_pos1 = subblock_position;
        blkptr = dablks.comms_block +(subblock_position*PARTITION_SIZE);
        memset(data, 0, PARTITION_SIZE);
        
        memcpy(data, blkptr, PARTITION_SIZE);
        evaluate_and_take_action(data, csstart_pos2);

        blkptr = NULL;
        toggle_bit(subblock_position, dablks.comms_block, 2);
    
    }

    sem_post(smlks.sem_lock_comms);
}


int get_data_from_processor(char *data, int *dsstart_pos)
{
    int subblock_position = -1;
    char *blkptr = NULL;
    
    sem_wait(smlks.sem_lock_datas);         
    subblock_position = get_subblock(dablks.datas_block , 1, *dsstart_pos);
    
    if(subblock_position >= 0) {

        *dsstart_pos = subblock_position;
        blkptr = dablks.datas_block +(subblock_position*PARTITION_SIZE);
        memset(data, 0, PARTITION_SIZE);
        memcpy(data, blkptr, PARTITION_SIZE);
        memset(blkptr, 0, PARTITION_SIZE);
        blkptr = NULL;
        toggle_bit(subblock_position, dablks.datas_block, 1);
    
    }

    sem_post(smlks.sem_lock_datas);

    return subblock_position > 0;

}


int send_data_over_network(unsigned int socketid, char *data) 
{
    printf("unimplemented send_data_over_network in sender.c");
}


int send_message(char *cid, int connection_socket, int *csstart_pos2) 
{
    int subblock_position = -1;
    char *blkptr = NULL;
    
    sem_wait(smlks.sem_lock_comms);         
    subblock_position = get_subblock(dablks.comms_block , 0, *csstart_pos2);
    
    if(subblock_position >= 0) {

        *csstart_pos2 = subblock_position;
        blkptr = dablks.datas_block +(subblock_position*PARTITION_SIZE);
        memset(blkptr, 0, PARTITION_SIZE);
        memcpy(blkptr, cid, 16);
        memcpy(blkptr, connection_socket, sizeof(int));
        toggle_bit(subblock_position, dablks.comms_block, 3);
    
    }

    sem_post(smlks.sem_lock_comms);
}


int run_sender() 
{
    int dsstart_pos = -1;
    int csstart_pos1 = -1;
    int csstart_pos2 = -1;
    int fd = 0;
    char data[PARTITION_SIZE];

    while (sender_status) {

        read_message(data, &csstart_pos1, &csstart_pos2);
        if(get_data_from_processor(data, &dsstart_pos))
            send_data_over_network(fd, data);

    }
}


int sender(void) 
{
    initialize_locks();
    get_shared_memory();
    run_sender();
    uninitialize_locks();
    detach_memory();   

    return 0;
}