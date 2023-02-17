#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> // contains important fucntionality and api used to create sockets
#include <sys/types.h>  // contains various types required to create a socket   
#include <netinet/in.h> // contains structures to store address information
#include "sender.h"
#include "partition.h"
#include "shared_memory.h"

datablocks dblks;
semlocks smlks;
int process_status = 1;

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


int get_shared_memory()
{
    dblks.datas_block = attach_memory_block(FILENAME, DATA_BLOCK_SIZE, PROJECT_ID_DATAS);
    dblks.comms_block = attach_memory_block(FILENAME, COMM_BLOCK_SIZE, PROJECT_ID_COMMS);

    if (!(dblks.datas_block && dblks.comms_block)) 
        return -1; 
    return 0;
}


int detach_memory()
{
    int status = 0;
    
    status = detach_memory_block(dblks.data_block);
    status = detach_memory_block(dblks.data_block);

    if (status == -1) return -1;
    
    return 0;
}


int initialize_locks() 
{
    int status = 0;

    smlks.sem_lock_datas = sem_open(SEM_LOCK_DATAS, O_CREAT, 0777, 1);
    smlks.sem_lock_comms = sem_open(SEM_LOCK_COMMS, O_CREAT, 0777, 1);

    if (slks.sem_lock_datar == SEM_FAILED || slks.sem_lock_commr == SEM_FAILED || slks.sem_lock_datas == SEM_FAILED || slks.sem_lock_comms = SEM_FAILED)
        status = -1;

    return status;
}


int uninitialize_locks()
{
    sem_close(smlks.sem_lock_datas);
    sem_close(smlks.sem_lock_comms);
}


int evaluate_and_take_action(char *data)
{
    int msg_status = -1
    unsigned char *ptr = data;
    unsigned int port_number = -1;
    unsigned int ipaddress = -1;
    int connection_socket = -1;
    int cid = -1;

    if (*ptr == 1) {
        // ippaddres
        // in this case create a connection
        *ptr++;
        cid = *(int *)ptr;

        ptr += 4;
        port_number = *(int *)ptr;
       
        ptr += 4;
        ipaddress = *(int *)ptr;
       
        connection_socket = create_connection(port_number, ipaddress);
        
        // communicate to database
        send_message(cid, connection_socket);

    }
    else if (*ptr == 2) {
        
        ptr++;

        cid = *(int *)ptr;
        connection_socket = *(int *)ptr;
        ptr += 4;
        
        msg_status = send_data_over_network(connection_socket, ptr);
        send_message(cid, msg_status);
        
    }

} 

int read_message(char *data, int *dsstart_pos) 
{

    int subblock_position = -1;
    char *blkptr = NULL;
    char data[PARTITION_SIZE];

    sem_wait(smlks.sem_lock_datas);         
    subblock_position = get_subblock(dblks.datas_block , 1, dsstart_pos);
    
    if(subblock_position >= 0) {

        dsstart_pos = subblock_position;
        blkptr = dblks.datas_block +(subblock_position*PARTITION_SIZE);
        memset(data, 0, PARTITION_SIZE);
        
        memcpy(data, blkptr, PARTITION_SIZE);
        evaluate_and_take_action(data);

        blkptr = NULL;
        toggle_bit(subblock_position, dblks.datas_block, 2);
    
    }

    sem_post(slks.sem_lock_datas);

}


int run_sender() 
{
    int dsstart_pos = -1;
    int csstart_pos1 = -1;
    int crstart_pos2 = -1;
    int fd = 0;
    char data[PARTITION_SIZE];

    while (process_status) {

        read_message(data);
        get_data_from_processor(fd, data);
        send_data();

    }
}


int main(void) 
{
    initialize_locks();
    get_shared_memory();
    run_sender();
    uninitialize_locks();
    detach_memeory();   

    return 0;
}