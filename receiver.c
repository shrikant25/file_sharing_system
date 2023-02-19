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

int *tbl;

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
            tbl[client_fd] = client_addr.sin_addr.s_addr;
            add_to_list(client_fd);
        }

    }
}


void read_socket(struct epoll_event event) 
{
    char buffer[4028];
    ssize_t bytes_read = 0;

    while (1) {
        bytes_read = read(event.data.fd, buffer, sizeof(buffer));
        if (bytes_read <=0) 
            return;
            //remove_from_list(event.data.fd);
            //printf("nothing to read");
        else {
        
            printf(" %d %s", tbl[event.data.fd],buffer);

            char v1[100];
            sprintf(v1, "%d", tbl[event.data.fd]);
            char *p[] = {v1, buffer};
            add(p);
            
            memset(buffer, 0, 4028);
        }
    }
}


int run_receiver() 
{
    int i;
    int act_events_cnt = -1;
    struct epoll_event events[s_info.maxevents];

    while (receiver_status) {
        
        act_events_cnt = epoll_wait(s_info.epoll_fd, events, s_info.maxevents, -1);
        if (act_events_cnt == -1) {
            perror("epoll wait failed");
            return;
        }

        char v[100];
        sprintf(v, "%d", act_events_cnt);
        char *p[] = {"69", v};
        add(p);
        for (i = 0; i<act_events_cnt; i++) {
            
            // error or hangup
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) { 
                perror("removing from list");
                remove_from_list(events[i].data.fd);
                continue;
            }
            else if (events[i].data.fd == s_info.servsoc_fd && (events[i].events & EPOLLIN)) {
                accept_connection();
            }
            else if (events[i].events & EPOLLIN){
                read_socket(events[i]);
            }
        }

    }
}
 
    
int send_to_processor(unsigned int ipaddress, char *data, int data_size, int *drstart_pos)
{
    int subblock_position = -1;
    char *blkptr = NULL;
    
    sem_wait(smlks.sem_lock_datar);         
    subblock_position = get_subblock(dblks.datar_block , 0, *drstart_pos);
    
    if(subblock_position >= 0) {

        *drstart_pos = subblock_position;
        blkptr = dblks.datar_block +(subblock_position*PARTITION_SIZE);
        memset(data, blkptr, PARTITION_SIZE);
        
        memcpy(blkptr, ipaddress, sizeof(ipaddress));
        blkptr += 4;
        memcpy(blkptr, data, data_size);
        memset(data, 0, PARTITION_SIZE);
        
        blkptr = NULL;
        toggle_bit(subblock_position, dblks.datar_block, 1);
    }

    sem_post(smlks.sem_lock_datar);
}


int read_message_from_processor(int *csstart_pos1, char *data)
{
    int subblock_position = -1;
    char *blkptr = NULL;
    
    sem_wait(smlks.sem_lock_commr);         
    subblock_position = get_subblock(dblks.datar_block , 1, *csstart_pos1);
    
    if(subblock_position >= 0) {

        *csstart_pos1 = subblock_position;
        blkptr = dblks.datar_block +(subblock_position*PARTITION_SIZE);
        memset(data, blkptr, PARTITION_SIZE);
        
        memcpy(data, 0, PARTITION_SIZE);
        memcpy(data, blkptr, PARTITION_SIZE);
        memset(blkptr, 0, PARTITION_SIZE);
        
        blkptr = NULL;
        toggle_bit(subblock_position, dblks.datar_block, 2);
    }
    
    sem_post(smlks.sem_lock_commr);
}


int send_message_to_processor(unsigned int fd, unsigned int ipaddress ,int *csstart_pos2)
{
    int subblock_position = -1;
    char *blkptr = NULL;
    
    sem_wait(smlks.sem_lock_commr);         
    subblock_position = get_subblock(dblks.commr_block , 0, *csstart_pos2);
    
    if(subblock_position >= 0) {

        *csstart_pos2 = subblock_position;
        blkptr = dblks.datar_block +(subblock_position*PARTITION_SIZE);
        
        memcpy(blkptr, fd, sizeof(fd));
        blkptr+=4;
        memcpy(blkptr, ipaddress, sizeof(ipaddress));
        
        toggle_bit(subblock_position, dblks.commr_block, 2);
    }
    
    sem_post(smlks.sem_lock_commr);
}


int init_receiver()
{
    s_info.maxevents = 1000;
    s_info.port = 7000;

    tbl = (int *)malloc(sizeof(int) * s_info.maxevents);
 
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


int close_reciever()
{
    close(s_info.epoll_fd);
    shutdown(s_info.servsoc_fd, 2);
 
    return 0;
}


int initialize_locks()
{
    int status = 0;

    smlks.sem_lock_datar = sem_open(SEM_LOCK_DATAR, O_CREAT, 0777, 1);
    smlks.sem_lock_commr = sem_open(SEM_LOCK_COMMR, O_CREAT, 0777, 1);

    if (smlks.sem_lock_datar == SEM_FAILED || smlks.sem_lock_commr == SEM_FAILED)
        status = -1;

    return status;
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


int receiver() 
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