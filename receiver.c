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
#include "hashtable.h"


char error[100];
server_info s_info;
PGconn *connection;
semlocks sem_lock_datar;
semlocks sem_lock_commr;
semlocks sem_lock_sigr;
datablocks datar_block;
datablocks commr_block;
hashtable htable;


int make_nonblocking (int active_fd) 
{
    // get current flags for file descriptor
    int flags = fcntl(active_fd, F_GETFL, 0);
    if (flags == -1) {
        memset(error, 0, sizeof(error));
        sprintf(error, "failed to get current flags %s, fd %d .", strerror(errno), active_fd);
        store_log(error);
        return -1;
    }

    // set the non blocking flag
    if( fcntl(active_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        memset(error, 0, sizeof(error));
        sprintf(error, "failed to set non blocking %s, fd %d .", strerror(errno), active_fd);
        store_log(error);
        return -1;
    }
    return 0;
}


int create_socket () 
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
        sprintf(error, "setting socket option reuseaddr failed %s, s_info.servsoc_fd : %d, optval : %d .", strerror(errno), s_info.servsoc_fd, optval);
        store_log(error);
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET; // set family
    servaddr.sin_port = htons(s_info.port); // set port
    servaddr.sin_addr.s_addr = htonl(s_info.ipaddress); // set ip

    // bind address to socketfd
    if (bind(s_info.servsoc_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        memset(error, 0, sizeof(error));
        sprintf(error, "binding address to socketfd failed %s, s_info.servsoc_fd : %d, servaddr : %d .", strerror(errno), s_info.servsoc_fd, servaddr.sin_addr);
        store_log(error);
        return -1;
    }

    return 0;
}


int add_to_list (int active_fd) 
{
    struct epoll_event event;
 
    memset(&event, 0, sizeof(event)); // set var to zero
    event.data.fd = active_fd; // add a file descriptor
    event.events = EPOLLIN | EPOLLET; // epollin to listen for incoming , epollet to specify it as edge triggered
 
    // try to add epoll variable to epoll listening list
    if (epoll_ctl(s_info.epoll_fd, EPOLL_CTL_ADD, active_fd, &event) == -1) {
        memset(error, 0, sizeof(error));
        sprintf(error, "adding to epoll list failed %s, epollfd : %d, fd to be added : %d .", strerror(errno), s_info.epoll_fd, active_fd);
        store_log(error);
        return -1;
    }

    return 0;
}


int remove_from_list (int active_fd) 
{
    struct epoll_event event;

    memset(&event, 0, sizeof(event)); // set var to zero
    event.data.fd = active_fd; // add file descriptor
    event.events = -1; // not of any use of set to -1
    
    // try to delete the file descriptor from epoll listening list
    if (epoll_ctl(s_info.epoll_fd, EPOLL_CTL_DEL, active_fd, &event) == -1) {
        memset(error, 0, sizeof(error));
        sprintf(error, "removing from epoll list failed %s, epollfd : %d, fd to be removed : %d .", strerror(errno), s_info.epoll_fd, active_fd);
        store_log(error);
        return -1;
    }
    return 0;
}


int accept_connection () 
{   
    int client_fd = -1;
    int optval;
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
                sprintf(error, "error accepting connection %s, servsocfd : %d .", strerror(errno), s_info.servsoc_fd);
                store_log(error);
                return -1;
            }
        }
        else {
            // get this data from database and store for buffer size
            // optval = 128 *1024;
            // if (setsockopt(client_fd, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval)) < 0) {
            //     memset(error, 0, sizeof(error));
            //     sprintf(error, "setting socket optinon  receive buffer size failed %s, s_info.servsoc_fd : %d, optval : %d .", strerror(errno), s_info.servsoc_fd, optval);
            //     store_log(error);
            //     return -1;
            // }

            if (make_nonblocking(client_fd) == -1) return -1;
            if (add_to_list(client_fd) == -1) {
                close(client_fd);
                return -1;
            } 
            
            memset(&rcvm, 0, sizeof(rcvm));
            rcvm.fd = client_fd;
            rcvm.ipaddr = htonl(client_addr.sin_addr.s_addr);
            rcvm.status = 2;
            
            send_message_to_processor(&rcvm);
        }
    }

    return 0;
}


int end_connection (int fd, struct sockaddr_in addr) 
{
    char key[17];
    receivers_message rcvm;
    socklen_t addrlen = sizeof(addr);
    
    if (remove_from_list(fd) == -1)
        return -1;
        
    memset(key, 0, sizeof(key));
    getsockname(fd, (struct sockaddr *) &addr, &addrlen);
    sprintf(key, "%ld", htonl(addr.sin_addr.s_addr));
    
    if (hdel(&htable, key) < 0) {
        memset(error, 0, sizeof(error));
        sprintf(error, "failed to delete key from table %s", key);
        store_log(error);
    }
    close(fd);

    memset(&rcvm, 0, sizeof(rcvm));
    rcvm.fd = fd;
    rcvm.ipaddr = 0;
    rcvm.status = 1;

    send_message_to_processor(&rcvm);
    return 0;
}


int run_receiver () 
{
    char key[17];
    int i, message_size, bytes_read, total_bytes_read, status;
    int act_events_cnt = -1;
    struct epoll_event events[s_info.maxevents];
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    newmsg_data nmsg;
    capacity_info cpif;

    while (1) {
        
        act_events_cnt = epoll_wait(s_info.epoll_fd, events, s_info.maxevents, 1000);

        if (get_message_from_processor(&cpif) != -1) {
            
            if ((status = hput(&htable, cpif.ipaddress, cpif.capacity)) != 0) {
                memset(error, 0, sizeof(error));
                sprintf(error, "failed to insert key, value in table status = %d ip = %s capacity = %d", status, cpif.ipaddress, cpif.capacity);
                store_log(error);
            }
            else {
                memset(error, 0, sizeof(error));
                sprintf(error, "succes inserting key %s, value %d in table", cpif.ipaddress, cpif.capacity);
                store_log(error);   
            }
        }


        if (act_events_cnt == -1) {
            memset(error, 0, sizeof(error));
            sprintf(error, "epoll wait failed %s, epollfd : %d .", strerror(errno), s_info.epoll_fd);
            store_log(error);
            return -1;
        }

        for (i = 0; i<act_events_cnt; i++) {
            
            // error or hangup
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) { 
                if (end_connection(events[i].data.fd, addr) == -1) { return -1; }  
                continue;
            
            }
            else if (events[i].data.fd == s_info.servsoc_fd && (events[i].events & EPOLLIN)) {
                accept_connection();
            }
            
            else if (events[i].events & EPOLLIN) {
                
                getsockname(events[i].data.fd, (struct sockaddr *) &addr, &addrlen);
                memset(key, 0, sizeof(key));
                sprintf(key, "%ld", htonl(addr.sin_addr.s_addr));
                message_size = hget(&htable, key);
                
                if (message_size >= 0) {
    
                    memset(&nmsg, 0, message_size);
                    nmsg.data1 = events[i].data.fd;
                    bytes_read = 0;
                    total_bytes_read = 0;
                    
                    while (1) { 

                        bytes_read = read(nmsg.data1, nmsg.data+total_bytes_read, message_size-total_bytes_read);
                        if (bytes_read <= 0) {
                            
                            memset(error, 0, sizeof(error));
                            sprintf(error, "read failed %s %d", strerror(errno), bytes_read);
                            store_log(error);
                            break;    
                        
                        }
                        else {

                            total_bytes_read += bytes_read;
                            memset(error, 0, sizeof(error));
                            sprintf(error, "total bytes read %d, byte read %d bytes", total_bytes_read, bytes_read);
                            store_log(error);
                        
                            if (total_bytes_read == message_size) {
                                
                                nmsg.data2 = total_bytes_read;
                                send_to_processor(&nmsg);
                                
                                total_bytes_read = 0;
                                memset(&nmsg, 0, message_size);
                                nmsg.data1 = events[i].data.fd;
                            }

                        }
                    }
                }
                else {
                    memset(error, 0, sizeof(error));
                    sprintf(error, "invalid size %d for fd %d ip %s", message_size, events[i].data.fd, key);
                    store_log(error);
                    if (end_connection(events[i].data.fd, addr) == -1) { return -1; }
                }
            }
            
        }

    }
    return 0;
}
 
    
int send_to_processor (newmsg_data *nmsg)
{
    int subblock_position = -1;
    unsigned char *blkptr = NULL;
    
    sem_wait(sem_lock_datar.var);         
    subblock_position = get_subblock(datar_block.var, 0, 3);

    if (subblock_position >= 0) {
        
        blkptr = datar_block.var + (TOTAL_PARTITIONS/8) + subblock_position * DPARTITION_SIZE;
        
        memset(blkptr, 0, DPARTITION_SIZE);
        memcpy(blkptr, nmsg, sizeof(newmsg_data));
        
        blkptr = NULL;
        toggle_bit(subblock_position, datar_block.var, 3);
        sem_post(sem_lock_sigr.var);
    } 
    else {
        store_log("failed to get empty block");
    }

    sem_post(sem_lock_datar.var);
    return subblock_position;
}


int get_message_from_processor (capacity_info *cpif) 
{
    int subblock_position = -1;
    char *blkptr = NULL;

    sem_wait(sem_lock_commr.var);
     
    subblock_position = get_subblock(commr_block.var, 1, 1);

    if (subblock_position >= 0) {
       
        memset(cpif, 0, sizeof(capacity_info));
        blkptr = commr_block.var + (TOTAL_PARTITIONS/8) + subblock_position * CPARTITION_SIZE;
        memcpy(cpif, blkptr, sizeof(capacity_info));
        memset(error, 0, sizeof(error));
        sprintf(error, "but why ip %s, cp %d", cpif->ipaddress, cpif->capacity);
        store_log(error);
        memcpy(blkptr, &cpif, sizeof(capacity_info));
        toggle_bit(subblock_position, commr_block.var, 1);
    } 

    sem_post(sem_lock_commr.var);
    return subblock_position;
}


int send_message_to_processor (receivers_message *rcvm)
{
    int subblock_position = -1;
    char *blkptr = NULL;
    char msg_type;
    
    sem_wait(sem_lock_commr.var);         
    subblock_position = get_subblock(commr_block.var, 0, 2);
    
    if (subblock_position >= 0) {

        blkptr = commr_block.var + (TOTAL_PARTITIONS/8) + subblock_position * CPARTITION_SIZE;
        
        memset(blkptr, 0, CPARTITION_SIZE);
        memcpy(blkptr, rcvm, sizeof(receivers_message));

        toggle_bit(subblock_position, commr_block.var, 2);
        sem_post(sem_lock_sigr.var);
    }
    else {
        store_log("failed to get empty block");
    }
    
    sem_post(sem_lock_commr.var);
    return subblock_position;
}


void store_log (char *logtext) 
{

    PGresult *res = NULL;
    char log[100];
    memset(log, 0, sizeof(log));
    strncpy(log, logtext, strlen(logtext));

    const char *const param_values[] = {log};
    const int paramLengths[] = {sizeof(log)};
    const int paramFormats[] = {0};
    int resultFormat = 0;
    
    res = PQexecPrepared(connection, dbs[0].statement_name, dbs[0].param_count, param_values, paramLengths, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        syslog(LOG_NOTICE, "logging failed %s , log %s\n", PQerrorMessage(connection), log);
    }

    PQclear(res);
}


int connect_to_database (char *conninfo) 
{   
    connection = PQconnectdb(conninfo);

    if (PQstatus(connection) != CONNECTION_OK) {
        syslog(LOG_NOTICE, "Connection to database failed: %s\n", PQerrorMessage(connection));
        return -1;
    }

    return 0;
}


int prepare_statements () 
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


int main (int argc, char *argv[]) 
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
       
        sscanf(buf, "SEM_LOCK_DATAR=%s\nSEM_LOCK_COMMR=%s\nSEM_LOCK_SIG_R=%s\nPROJECT_ID_DATAR=%d\nPROJECT_ID_COMMR=%d\nUSERNAME=%s\nDBNAME=%s\nPORT=%d\nIPADDRESS=%lu", sem_lock_datar.key, sem_lock_commr.key, sem_lock_sigr.key, &datar_block.key, &commr_block.key, username, dbname, &s_info.port, &s_info.ipaddress);
    }
    else {
        syslog(LOG_NOTICE, "failed to read configuration file");
        return -1;
    }

    close(conffd);

    sprintf(db_conn_command, "user=%s dbname=%s", username, dbname);

    if (connect_to_database(db_conn_command) == -1) { return -1; }
    if (prepare_statements() == -1) { return -1; }     
    
    // get lock variable for lock on data sharing memory
    sem_lock_datar.var = sem_open(sem_lock_datar.key, O_CREAT, 0777, 1);

    // get lock variable for lock on message sharing memory
    sem_lock_commr.var = sem_open(sem_lock_commr.key, O_CREAT, 0777, 1);
    
    // get lock variable for signaling
    sem_lock_sigr.var = sem_open(sem_lock_sigr.key, O_CREAT, 0777, 0);
    
    if (sem_lock_sigr.var == SEM_FAILED || sem_lock_datar.var == SEM_FAILED || sem_lock_commr.var == SEM_FAILED) {
        store_log("failed to intialize locks");
        status = -1;
    }

    // attach memroy block for data sharing
    datar_block.var = attach_memory_block(FILENAME_R, DATA_BLOCK_SIZE, datar_block.key);
    
    // attach memroy block for message sharing
    commr_block.var = attach_memory_block(FILENAME_R, COMM_BLOCK_SIZE, commr_block.key);
    
    if (!(datar_block.var && commr_block.var)) {
        store_log("failed to get shared memory");
        return -1; 
    }    
    
    s_info.maxevents = 1000; // maxevent that will taken from kernel to process buffer
 
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
    
    if (hcreate_table(&htable, 1000) != 0) {
        store_log("failed to create fd-capacity table");
        return -1;
    }

    run_receiver(); // recieve connections, data and communicate with database
 
    PQfinish(connection); // close connection to db
   
    close(s_info.epoll_fd); // close epoll instance
    shutdown(s_info.servsoc_fd, 2); // close server socket
   
    sem_close(sem_lock_commr.var); // unlink lock used for communication
    sem_close(sem_lock_datar.var); // unlink lock used for data sharing
    sem_close(sem_lock_sigr.var);

    detach_memory_block(datar_block.var); // detach memory used for data sharing
    detach_memory_block(commr_block.var); // detach memory used for communication
    
    return 0;
}