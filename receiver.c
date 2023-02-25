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
    int flags = fcntl(active_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("failed to get file descriptor flags");
        return;
    }

    if( fcntl(active_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("failed to set file descriptor flags");
        return;
    }
}


void create_socket() 
{
    int optval;
    struct sockaddr_in servaddr;
    
    s_info.servsoc_fd = socket(AF_INET, SOCK_STREAM, 0);
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
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(s_info.port);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s_info.servsoc_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("bind error");
        exit(1);
    }
}


void create_epoll() 
{
    s_info.epoll_fd = epoll_create1(0);
    if (s_info.epoll_fd == -2) {
        perror("failed to create epoll instance");
        return;
    }
}


void add_to_list(int active_fd) 
{
    struct epoll_event event;
 
    memset(&event, 0, sizeof(event));
    event.data.fd = active_fd;
    event.events = EPOLLIN | EPOLLET;
 
    if (epoll_ctl(s_info.epoll_fd, EPOLL_CTL_ADD, active_fd, &event) == -1) {
        perror("error adding");
        return;
    }
}


void remove_from_list(int active_fd) 
{
    struct epoll_event event;

    memset(&event, 0, sizeof(event));
    event.data.fd = active_fd;
    event.events = -1;
    
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
    ssize_t bytes_read = 0;

    while (1) {
        bytes_read = read(event.data.fd, buffer, sizeof(buffer));
        if (bytes_read <=0) 
            return;
        else {
            while (send_to_processor(event.data.fd, buffer, bytes_read) == -1);
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
        
        read_message_from_processor(data); // todo

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
    subblock_position = get_subblock(dblks.datar_block , 0);
    
    if (subblock_position >= 0) {

        blkptr = dblks.datar_block + 3 + subblock_position * DPARTITION_SIZE;
        memset(blkptr, 0, DPARTITION_SIZE);
        
        memcpy(blkptr, &socketid, sizeof(socketid));
        
        blkptr += 4;
        memcpy(blkptr, &data_size, sizeof(data_size));
        
        blkptr += 4;
        memcpy(blkptr, data, data_size);
        
        memset(data, 0, DPARTITION_SIZE);
        blkptr = NULL;
        toggle_bit(subblock_position, dblks.datar_block, 1);
    }

    sem_post(smlks.sem_lock_datar);
    return subblock_position;
}


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


int send_message_to_processor(unsigned int fd, unsigned int ipaddress)
{
    int subblock_position = -1;
    char *blkptr = NULL;
    char msg_type;
    
    sem_wait(smlks.sem_lock_commr);         
    subblock_position = get_subblock2(dblks.commr_block , 0, 1);
    
    if (subblock_position >= 0) {

        blkptr = dblks.commr_block + 4 + subblock_position * CPARTITION_SIZE;

        msg_type = ipaddress == 0 ? '0' : '1';
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
    s_info.maxevents = 1000;
    s_info.port = 7000;

    create_socket();
    make_nonblocking(s_info.servsoc_fd);
 
    // somaxconn is defined in socket.h
    if (listen(s_info.servsoc_fd, SOMAXCONN) < 0) {
        perror("failed to listen on port");
        return 1;
    }
 
    create_epoll();
    add_to_list(s_info.servsoc_fd);
}


int close_receiver()
{
    close(s_info.epoll_fd);
    shutdown(s_info.servsoc_fd, 2);
 
    return 0;
}


int initialize_locks()
{
 
    smlks.sem_lock_datar = sem_open(SEM_LOCK_DATAR, O_CREAT, 0777, 1);
    smlks.sem_lock_commr = sem_open(SEM_LOCK_COMMR, O_CREAT, 0777, 1);

    if (smlks.sem_lock_datar == SEM_FAILED || smlks.sem_lock_commr == SEM_FAILED)
        return -1;

    return 0;
}


int get_shared_memeory()
{
    dblks.datar_block = attach_memory_block(FILENAME, DATA_BLOCK_SIZE, PROJECT_ID_DATAR);
    dblks.commr_block = attach_memory_block(FILENAME, COMM_BLOCK_SIZE, PROJECT_ID_COMMR);

    if (!(dblks.datar_block && dblks.commr_block))
        return -1;
    
    return 0;
}


int uninitialize_locks()
{    
    sem_close(smlks.sem_lock_commr);
    sem_close(smlks.sem_lock_datar);
}


int detach_memory()
{
    int status = 0;

    status = detach_memory_block(dblks.datar_block);
    status = detach_memory_block(dblks.commr_block);

    return status;
}


int main(void) 
{   
    initialize_locks();
    get_shared_memeory();   
    init_receiver();
    run_receiver();
    close_receiver();
    uninitialize_locks();
    detach_memory();
    
    return 0;
}