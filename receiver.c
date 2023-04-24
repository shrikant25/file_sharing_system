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
#include <syslog.h>
#include <libpq-fe.h>
#include <libpq/libpq-fs.h>
#include "receiver.h"
#include "shared_memory.h"
#include "partition.h"


server_info s_info;
datablocks dblks;
semlocks smlks;
PGconn *connection;
int receiver_status = 1;
char error[100];

int make_nonblocking(int active_fd) 
{
    // get current flags for file descriptor
    int flags = fcntl(active_fd, F_GETFL, 0);
    if (flags == -1) {
        memset(error, 0, sizeof(error));
        sprintf(error, "failed to get current flags %s, fd %d", strerror(errno), active_fd);
        store_log(error);
        return -1;
    }

    // set the non blocking flag
    if( fcntl(active_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        memset(error, 0, sizeof(error));
        sprintf(error, "failed to set non blocking %s, fd %d", strerror(errno), active_fd);
        store_log(error);
        return -1;
    }
    return 0;
}


int create_socket() 
{
    int optval;
    struct sockaddr_in servaddr;
    
    s_info.servsoc_fd = socket(AF_INET, SOCK_STREAM, 0); // create socket for servet
    if (s_info.servsoc_fd == -1) {
        store_log("creating socket failed");
        return -1;
    }

    // if incase the process dies, then after restart if the socket is already in use
    // then bind to that socket
    optval = 1;
    if (setsockopt(s_info.servsoc_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        memset(error, 0, sizeof(error));
        sprintf(error, "setting socket option reuseaddr failed %s, s_info.servsoc_fd : %d, optval : %d", strerror(errno), s_info.servsoc_fd, optval);
        store_log(error);
        return -1;
    }

    optval = 128 *1024;
    if (setsockopt(s_info.servsoc_fd, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval)) < 0) {
        memset(error, 0, sizeof(error));
        sprintf(error, "setting socket optinon  receive buffer size failed %s, s_info.servsoc_fd : %d, optval : %d", strerror(errno), s_info.servsoc_fd, optval);
        store_log(error);
        return -1;
    }


    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET; // set family
    servaddr.sin_port = htons(s_info.port); // set port
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // set ip

    // bind address to socketfd
    if (bind(s_info.servsoc_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        memset(error, 0, sizeof(error));
        sprintf(error, "binding address to socketfd failed %s, s_info.servsoc_fd : %d, servaddr : %d", strerror(errno), s_info.servsoc_fd, servaddr.sin_addr);
        store_log(error);
        return -1;
    }

    return 0;
}


int add_to_list(int active_fd) 
{
    struct epoll_event event;
 
    memset(&event, 0, sizeof(event)); // set var to zero
    event.data.fd = active_fd; // add a file descriptor
    event.events = EPOLLIN | EPOLLET; // epollin to listen for incoming , epollet to specify it as edge triggered
 
    // try to add epoll variable to epoll listening list
    if (epoll_ctl(s_info.epoll_fd, EPOLL_CTL_ADD, active_fd, &event) == -1) {
        memset(error, 0, sizeof(error));
        sprintf(error, "adding to epoll list failed %s, epollfd : %d, fd to be added : %d", strerror(errno), s_info.epoll_fd, active_fd);
        store_log(error);
        return -1;
    }

    return 0;
}


int remove_from_list(int active_fd) 
{
    struct epoll_event event;

    memset(&event, 0, sizeof(event)); // set var to zero
    event.data.fd = active_fd; // add file descriptor
    event.events = -1; // not of any use of set to -1
    
    // try to delete the file descriptor from epoll listening list
    if (epoll_ctl(s_info.epoll_fd, EPOLL_CTL_DEL, active_fd, &event) == -1) {
        memset(error, 0, sizeof(error));
        sprintf(error, "removing from epoll list failed %s, epollfd : %d, fd to be removed : %d", strerror(errno), s_info.epoll_fd, active_fd);
        store_log(error);
        return -1;
    }
    return 0;
}


int accept_connection() 
{   
    int client_fd = -1;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    receivers_message rcvm;

    while (1) { // keep accepting connections as long as there are connections in queue
    
        client_addr_len = sizeof(client_addr);
        
        client_fd = accept(s_info.servsoc_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { // no connections are present
                return 0;
            }
            else {
                memset(error, 0, sizeof(error));
                sprintf(error, "error accepting connection %s, servsocfd : %d", strerror(errno), s_info.servsoc_fd);
                store_log(error);
                return -1;
            }
        }
        else{
            if (make_nonblocking(client_fd) == -1) return -1;
            if (add_to_list(client_fd) == -1) {
                close(client_fd);
                return -1;
            } 
            
            memset(&rcvm, 0, sizeof(rcvm));
            rcvm.fd = client_fd;
            rcvm.ipaddr = htonl(client_addr.sin_addr.s_addr);
            rcvm.status = 1;
            
            send_message_to_processor(&rcvm);
        }
    }

    return 0;
}


int run_receiver() 
{
    int i;
    int act_events_cnt = -1;
    struct epoll_event events[s_info.maxevents];
    int bytes_read = 0;
    int total_bytes_read = 0;
    newmsg_data nmsg;
    receivers_message rcvm;

    while (receiver_status) {
        
      //  read_message_from_processor(data); // todo
        act_events_cnt = epoll_wait(s_info.epoll_fd, events, s_info.maxevents, -1);
        if (act_events_cnt == -1) {
            memset(error, 0, sizeof(error));
            sprintf(error, "epoll wait failed %s, epollfd : %d", strerror(errno), s_info.epoll_fd);
            store_log(error);
            return -1;
        }

        for (i = 0; i<act_events_cnt; i++) {
            
            // error or hangup
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) { 
                
                close(events[i].data.fd);
                if (remove_from_list(events[i].data.fd) == -1)
                    return -1;
                
                memset(&rcvm, 0, sizeof(rcvm));
                rcvm.fd = events[i].data.fd;
                rcvm.ipaddr = 0;
                rcvm.status = 2;

                send_message_to_processor(&rcvm);
                continue;
            
            }
            else if (events[i].data.fd == s_info.servsoc_fd && (events[i].events & EPOLLIN)) 
                accept_connection();
            
            else if (events[i].events & EPOLLIN){

                memset(&nmsg, 0, MESSAGE_SIZE);
                nmsg.data1 = events[i].data.fd;
                bytes_read = 0;
                total_bytes_read = 0;

                while (total_bytes_read < MESSAGE_SIZE) {
                    
                    bytes_read = read(nmsg.data1, nmsg.data+total_bytes_read, MESSAGE_SIZE);
                    
                    if (bytes_read == 0) {
                       
                        memset(&rcvm, 0, sizeof(rcvm));
                        rcvm.fd = events[i].data.fd;
                        rcvm.ipaddr = 0;
                        rcvm.status = 2;
                       
                        if (remove_from_list(events[i].data.fd) == -1)
                            return -1;
                        close(events[i].data.fd);

                        send_message_to_processor(&rcvm);     
                        break;
                    }

                    total_bytes_read += bytes_read;
                    
                }

                if (total_bytes_read == MESSAGE_SIZE) {
                    send_to_processor(&nmsg);
                }

            }
            
        }

    }
    return 0;
}
 
    
int send_to_processor(newmsg_data *nmsg)
{
    int subblock_position = -1;
    char *blkptr = NULL;
    
    sem_wait(smlks.sem_lock_datar);         
    subblock_position = get_subblock(dblks.datar_block, 0, 3);

    if (subblock_position >= 0) {
        
        blkptr = dblks.datar_block + (TOTAL_PARTITIONS/8) + subblock_position * DPARTITION_SIZE;
        
        memset(blkptr, 0, DPARTITION_SIZE);
        memcpy(blkptr, nmsg, sizeof(newmsg_data));
        
        blkptr = NULL;
        toggle_bit(subblock_position, dblks.datar_block, 3);
        sem_post(smlks.sem_lock_sigr);
    } 
    else {
        store_log("failed to get empty block");
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
    subblock_position = get_subblock2(dblks.commr_block, 1, 1);
    
    if (subblock_position >= 0) {

        blkptr = dblks.commr_block + (TOTAL_PARTITIONS/8) + subblock_position * CPARTITION_SIZE;
        memset(data, 0, CPARTITION_SIZE);
        
        memcpy(data, blkptr, CPARTITION_SIZE);
        memset(blkptr, 0, CPARTITION_SIZE);
        
        blkptr = NULL; 
        toggle_bit(subblock_position, dblks.commr_block, 1);
    }
    
    sem_post(smlks.sem_lock_commr);
    return subblock_position;
}
*/

int send_message_to_processor(receivers_message *rcvm)
{
    int subblock_position = -1;
    char *blkptr = NULL;
    char msg_type;
    
    sem_wait(smlks.sem_lock_commr);         
    subblock_position = get_subblock(dblks.commr_block, 0, 2);
    
    if (subblock_position >= 0) {

        blkptr = dblks.commr_block + (TOTAL_PARTITIONS/8) + subblock_position * CPARTITION_SIZE;
        
        memset(blkptr, 0, CPARTITION_SIZE);
        memcpy(blkptr, rcvm, sizeof(receivers_message));

        toggle_bit(subblock_position, dblks.commr_block, 2);
        sem_post(smlks.sem_lock_sigr);
    }
    else {
        store_log("failed to get empty block");
    }
    
    sem_post(smlks.sem_lock_commr);
    return subblock_position;
}


void store_log(char *logtext) {

    PGresult *res = NULL;
    char log[100];
    strncpy(log, logtext, strlen(logtext));

    const char *const param_values[] = {log};
    const int paramLengths[] = {sizeof(log)};
    const int paramFormats[] = {0};
    int resultFormat = 0;
    
    res = PQexecPrepared(connection, "r_storelog", 1, param_values, paramLengths, paramFormats, 0);
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

    int status = 0;
    if (connect_to_database() == -1) { return -1; }
    if (prepare_statements() == -1) { return -1; }   

    // get lock variable for lock on data sharing memory
    smlks.sem_lock_datar = sem_open(SEM_LOCK_DATAR, O_CREAT, 0777, 1);

    // get lock variable for lock on message sharing memory
    smlks.sem_lock_commr = sem_open(SEM_LOCK_COMMR, O_CREAT, 0777, 1);

    // get lock variable for signaling
    smlks.sem_lock_sigr = sem_open(SEM_LOCK_SIG_R, O_CREAT, 0777, 0);

    if (smlks.sem_lock_sigr == SEM_FAILED || smlks.sem_lock_datar == SEM_FAILED || smlks.sem_lock_commr == SEM_FAILED) { 
        store_log("failed to initialize locks");
        return -1;
    }
    

    // filename and projectid are present in shared_memory.h
    // block sizes are present in partition.h

    // attach memroy block for data sharing
    dblks.datar_block = attach_memory_block(FILENAME_R, DATA_BLOCK_SIZE, PROJECT_ID_DATAR);
    
    // attach memroy block for message sharing
    dblks.commr_block = attach_memory_block(FILENAME_R, COMM_BLOCK_SIZE, PROJECT_ID_COMMR);

    if (!(dblks.datar_block && dblks.commr_block)) {
        store_log("failed to get/attach shared memory");
        return -1;
    }
    
    
    s_info.maxevents = 1000; // maxevent that will taken from kernel to process buffer
    s_info.port = 7000; // server port
 
    if (create_socket() == -1) { return -1; } // create socket
    if (make_nonblocking(s_info.servsoc_fd) == -1) { return -1; } // make the server socket file descriptor as non blocking
 
    // somaxconn is defined in socket.h
    if (listen(s_info.servsoc_fd, SOMAXCONN) < 0) { // listen for incoming connections
        store_log("failed to listen on port");
        return -1;
    }
 
    
    s_info.epoll_fd = epoll_create1(0); // create epoll instance
    if (s_info.epoll_fd == -2) {
        store_log("failed to create epoll instance");
        return -1;
    }

    if (add_to_list(s_info.servsoc_fd) == -1) { return -1; } // add socket file descriptor to epoll list
    
    run_receiver(); // recieve connections, data and communicate with database
 
    PQfinish(connection); // close connection to db
   
    close(s_info.epoll_fd); // close epoll instance
    shutdown(s_info.servsoc_fd, 2); // close server socket
   
    sem_close(smlks.sem_lock_commr); // unlink lock used for communication
    sem_close(smlks.sem_lock_datar); // unlink lock used for data sharing
    sem_close(smlks.sem_lock_sigr);

    detach_memory_block(dblks.datar_block); // detach memory used for data sharing
    detach_memory_block(dblks.commr_block); // detach memory used for communication
    
    return 0;
}