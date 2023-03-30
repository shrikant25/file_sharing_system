#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <netinet/in.h>
#include <libpq-fe.h>
#include <libpq/libpq-fs.h>
#include "receiver.h"
#include "shared_memory.h"
#include "partition.h"

server_info s_info;
datablocks dblks;
semlocks smlks;

int receiver_status = 1;


void make_nonblocking(int active_fd) 
{
    // get current flags for file descriptor
    int flags = fcntl(active_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("failed to get file descriptor flags");
        return;
    }

    // set the non blocking flag
    if( fcntl(active_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("failed to set file descriptor flags");
        return;
    }
}


void create_socket() 
{
    int optval;
    struct sockaddr_in servaddr;
    
    s_info.servsoc_fd = socket(AF_INET, SOCK_STREAM, 0); // create socket for servet
    if (s_info.servsoc_fd == -1) {
        perror("socket error");
        exit(1);
    }

    // if incase the process dies, then after restart if the socket is already in use
    // then bind to that socket
    optval = 1;
    if (setsockopt(s_info.servsoc_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsokopt");
        return;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET; // set family
    servaddr.sin_port = htons(s_info.port); // set port
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // set ip

    // bind address to socketfd
    if (bind(s_info.servsoc_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("bind error");
        exit(1);
    }
}


void create_epoll() 
{
    s_info.epoll_fd = epoll_create1(0); // create epoll instance
    if (s_info.epoll_fd == -2) {
        perror("failed to create epoll instance");
        return;
    }
}


void add_to_list(int active_fd) 
{
    struct epoll_event event;
 
    memset(&event, 0, sizeof(event)); // set var to zero
    event.data.fd = active_fd; // add a file descriptor
    event.events = EPOLLIN | EPOLLET; // epollin to listen for incoming , epollet to specify it as edge triggered
 
    // try to add epoll variable to epoll listening list
    if (epoll_ctl(s_info.epoll_fd, EPOLL_CTL_ADD, active_fd, &event) == -1) {
        perror("error adding");
        return;
    }
}


void remove_from_list(int active_fd) 
{
    struct epoll_event event;

    memset(&event, 0, sizeof(event)); // set var to zero
    event.data.fd = active_fd; // add file descriptor
    event.events = -1; // not of any use of set to -1
    
    // try to delete the file descriptor from epoll listening list
    if (epoll_ctl(s_info.epoll_fd, EPOLL_CTL_DEL, active_fd, &event) == -1) {
        perror("error removing");
        return;
    }
}


void accept_connection() 
{   
    int client_fd = -1;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;

    while (1) { // keep accepting connections as long as there are connections in queue
    
        client_addr_len = sizeof(client_addr);
        
        client_fd = accept(s_info.servsoc_fd, (struct sockaddr *)&client_addr, &client_addr_len);
       
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) // no connections are present
                return;
            else {
                perror("error while acceping client connection"); // some other error occured
                return;
            }
        }
        else{
            make_nonblocking(client_fd);
            add_to_list(client_fd);
            while (send_message_to_processor(client_fd, client_addr.sin_addr.s_addr) == -1);
        }
    }
}


void read_socket(struct epoll_event event) 
{
    char buffer[1024 * 128];
    ssize_t bytes_read;
    int msg_size = 1024 * 128;

    while (1) {

        bytes_read = 0;
        memset(buffer, 0, sizeof(buffer));

        bytes_read = read(event.data.fd, buffer, sizeof(buffer));
        if (bytes_read <=0) 
            return;
        else {
                while(send_to_processor(event.data.fd, buffer, msg_size) == -1);  
        }   
    }
}


int run_receiver() 
{
    int i;
    int act_events_cnt = -1;
    struct epoll_event events[s_info.maxevents];
    char data[CPARTITION_SIZE];

    while (receiver_status) {
        
      //  read_message_from_processor(data); // todo

        act_events_cnt = epoll_wait(s_info.epoll_fd, events, s_info.maxevents, -1);
        if (act_events_cnt == -1) {
            perror("epoll wait failed");
            return -1;
        }

        for (i = 0; i<act_events_cnt; i++) {
            
            // error or hangup
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) { 
                
                printf("removing from list");
                remove_from_list(events[i].data.fd);
                while (send_message_to_processor(events[i].data.fd, 0) == -1);
                continue;
            
            }
            else if (events[i].data.fd == s_info.servsoc_fd && (events[i].events & EPOLLIN)) 
                accept_connection();
            
            else if (events[i].events & EPOLLIN)
                read_socket(events[i]);
            
        }

    }
}
 
    
int send_to_processor(unsigned int socketid, char *data, int data_size)
{
    int subblock_position = -1;
    char *blkptr = NULL;
    
    sem_wait(smlks.sem_lock_datar);         
    subblock_position = get_subblock(dblks.datar_block, 0);
    
    if (subblock_position >= 0) {

        blkptr = dblks.datar_block + 3 + subblock_position * DPARTITION_SIZE;
        memset(blkptr, 0, DPARTITION_SIZE);
        
        memcpy(blkptr, &socketid, sizeof(socketid));
        
        blkptr += 4;
        memcpy(blkptr, data, data_size);
        
        blkptr = NULL;
        toggle_bit(subblock_position, dblks.datar_block, 1);
    }

    sem_post(smlks.sem_lock_datar);
    return subblock_position;
}

/*
int read_message_from_processor(char *data)
{
    int subblock_position = -1;
    char *blkptr = NULL;
    
    sem_wait(smlks.sem_lock_commr);         
    subblock_position = get_subblock2(dblks.commr_block, 1, 0);
    
    if (subblock_position >= 0) {

        blkptr = dblks.commr_block + 2 + subblock_position * CPARTITION_SIZE;
        memset(data, 0, CPARTITION_SIZE);
        
        memcpy(data, blkptr, CPARTITION_SIZE);
        memset(blkptr, 0, CPARTITION_SIZE);
        
        blkptr = NULL; 
        toggle_bit(subblock_position, dblks.commr_block, 2);
    }
    
    sem_post(smlks.sem_lock_commr);
    return subblock_position;
}
*/

int send_message_to_processor(unsigned int fd, unsigned long ipaddress)
{
    int subblock_position = -1;
    char *blkptr = NULL;
    char msg_type;
    
    sem_wait(smlks.sem_lock_commr);         
    subblock_position = get_subblock2(dblks.commr_block, 0, 1);
    
    if (subblock_position >= 0) {

        blkptr = dblks.commr_block + 4 + subblock_position * CPARTITION_SIZE;
        memset(blkptr, 0, CPARTITION_SIZE);

        msg_type = ipaddress == 0 ? '2' : '1';
        memcpy(blkptr, &msg_type, sizeof(msg_type));
   
        blkptr++;         
        memcpy(blkptr, &fd, sizeof(fd));
         
        if (msg_type == '1') { 
            blkptr+=4;
            memcpy(blkptr, &ipaddress, sizeof(ipaddress));
        }

        toggle_bit(subblock_position, dblks.commr_block, 3);
    }
    
    sem_post(smlks.sem_lock_commr);
    return subblock_position;
}


int init_receiver()
{
    s_info.maxevents = 1000; // maxevent that will taken from kernel to process buffer
    s_info.port = 7000; // server port
 
    create_socket(); // create socket
    make_nonblocking(s_info.servsoc_fd); // make the server socket file descriptor as non blocking
 
    // somaxconn is defined in socket.h
    if (listen(s_info.servsoc_fd, SOMAXCONN) < 0) { // listen for incoming connections
        perror("failed to listen on port");
        return 1;
    }
 
    create_epoll(); // create
    add_to_list(s_info.servsoc_fd); // add socket file descriptor to epoll list
}


int close_receiver()
{
    close(s_info.epoll_fd); // close epoll instance
    shutdown(s_info.servsoc_fd, 2); // close server socket
 
    return 0;
}


int initialize_locks()
{
    // get lock variable for lock on data sharing memory
    smlks.sem_lock_datar = sem_open(SEM_LOCK_DATAR, O_CREAT, 0777, 1);

    // get lock variable for lock on message sharing memory
    smlks.sem_lock_commr = sem_open(SEM_LOCK_COMMR, O_CREAT, 0777, 1);

    if (smlks.sem_lock_datar == SEM_FAILED || smlks.sem_lock_commr == SEM_FAILED)
        return -1;

    return 0;
}


int get_shared_memeory()
{
    // filename and projectid are present in shared_memory.h
    // block sizes are present in partition.h

    // attach memroy block for data sharing
    dblks.datar_block = attach_memory_block(FILENAME, DATA_BLOCK_SIZE, PROJECT_ID_DATAR);
    
    // attach memroy block for message sharing
    dblks.commr_block = attach_memory_block(FILENAME, COMM_BLOCK_SIZE, PROJECT_ID_COMMR);

    if (!(dblks.datar_block && dblks.commr_block))
        return -1;
    
    return 0;
}


int uninitialize_locks()
{    
    sem_close(smlks.sem_lock_commr); // unlink lock used for communication
    sem_close(smlks.sem_lock_datar); // unlink lock used for data sharing
}


int detach_memory()
{
    int status = 0;

    status = detach_memory_block(dblks.datar_block); // detach memory used for data sharing
    status = detach_memory_block(dblks.commr_block); // detach memory used for communication
    
    //    instead of returning status, status can be checked here and appropriate action can be taken
    

    return status;  // return status
}


int main(void) 
{   
    initialize_locks(); // link variables to use as locks
    get_shared_memeory(); // connect to shared memory
    init_receiver(); // creates server instance
    run_receiver(); // recieve connections, data and communicate with database
    close_receiver(); // close server instance
    uninitialize_locks(); // unlink variables from locks
    detach_memory(); // detach memory
    
    return 0;
}