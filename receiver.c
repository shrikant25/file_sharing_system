#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <stdint.h>
#include <netinet/in.h>
#include "receiver.h"
#include "shared_memory.h"
#include "partition.h"
#include "hashtable.h"

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
        storelog("%s%s%s%d", "failed to get current flags : ",  strerror(errno), " fd : ", active_fd);
        return -1;
    }

    // set the non blocking flag
    if( fcntl(active_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        storelog("%s%s%s%d", "failed to set non blocking : ", strerror(errno),  " fd : ", active_fd);
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
        storelog("%s", "receiver creating socket failed");
        return -1;
    }

    // if incase the process dies, then after restart if the socket is already in use
    // then bind to that socket
    optval = 1;
    if (setsockopt(s_info.servsoc_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        storelog("%s%s%s%d%s%d", "setting socket option reuseaddr failed :", strerror(errno), "s_info.servsoc_fd : ", s_info.servsoc_fd,  "optval : ", optval);
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET; // set family
    servaddr.sin_port = htons(s_info.port); // set port
    servaddr.sin_addr.s_addr = htonl(s_info.ipaddress); // set ip

    // bind address to socketfd
    if (bind(s_info.servsoc_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        storelog("%s%s%s%d%s%d", "binding address to socketfd failed : ", strerror(errno), "s_info.servsoc_fd : ", s_info.servsoc_fd, "servaddr : ", servaddr.sin_addr);
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
        storelog("%s%s%s%d%s%d", "adding to epoll list failed : ", strerror(errno), " epollfd : ", s_info.epoll_fd, " fd to be added : ", active_fd);
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
        storelog("%s%s%s%d%s%d", "removing from epoll list failed : ", strerror(errno), " epollfd : ", s_info.epoll_fd, " fd to be removed : ", active_fd);
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
                storelog("%s%s%s%d", "error accepting connection : ",  strerror(errno), " servsocfd : ", s_info.servsoc_fd);
                return -1;
            }
        }
        else {

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
        storelog("%s%s", "failed to delete key from table : ", key);
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
                storelog("%s%d%s%s%s%d", "failed to insert key, value in table status : ",  status ," ip : ", cpif.ipaddress,  "capacity : ", cpif.capacity);
            }
          
        }

        if (act_events_cnt == -1) {
            storelog("%s%s%s%d", "epoll wait failed : ", strerror(errno), " epollfd : ", s_info.epoll_fd);
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
                            break;    
                        }
                        else {

                            total_bytes_read += bytes_read;
                        
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
                    storelog("%s%d%s%d%s%s", "invalid size : ", message_size,  " for fd : ", events[i].data.fd,  " ip : ", key);
                    if (end_connection(events[i].data.fd, addr) == -1) { return -1; }
                }
            }
            
        }

    }
    return 0;
}
 
 
void send_to_processor (newmsg_data *nmsg)
{
    int subblock_position = -1;
    int attempts = 3;
    unsigned char *blkptr = NULL;
    
    sem_wait(sem_lock_datar.var);
    do {         
        subblock_position = get_subblock(datar_block.var, 0, 3);

        if (subblock_position >= 0) {
            
            blkptr = datar_block.var + (TOTAL_PARTITIONS/8) + subblock_position * DPARTITION_SIZE;
            
            memset(blkptr, 0, DPARTITION_SIZE);
            memcpy(blkptr, nmsg, sizeof(newmsg_data));
            
            blkptr = NULL;
            toggle_bit(subblock_position, datar_block.var, 3);
            sem_post(sem_lock_sigr.var);
        } 
        attempts -= 1;
    } while (attempts > 0 && subblock_position == -1);    

    sem_post(sem_lock_datar.var);
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
        memcpy(blkptr, &cpif, sizeof(capacity_info));
        toggle_bit(subblock_position, commr_block.var, 1);
    } 

    sem_post(sem_lock_commr.var);
    return subblock_position;
}


void send_message_to_processor (receivers_message *rcvm)
{
    int subblock_position = -1;
    char *blkptr = NULL;
    char msg_type;
    int attempts = 3;
    
    sem_wait(sem_lock_commr.var);         
    do {
        subblock_position = get_subblock(commr_block.var, 0, 2);
        
        if (subblock_position >= 0) {

            blkptr = commr_block.var + (TOTAL_PARTITIONS/8) + subblock_position * CPARTITION_SIZE;
            
            memset(blkptr, 0, CPARTITION_SIZE);
            memcpy(blkptr, rcvm, sizeof(receivers_message));

            toggle_bit(subblock_position, commr_block.var, 2);
            sem_post(sem_lock_sigr.var);
        }

        attempts -= 1;  
    } while (attempts > 0 && subblock_position == -1);  
    sem_post(sem_lock_commr.var);
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
        storelog("%s", "receiver failed to intialize locks");
        status = -1;
    }

    // attach memroy block for data sharing
    datar_block.var = attach_memory_block(FILENAME_R, DATA_BLOCK_SIZE, datar_block.key);
    
    // attach memroy block for message sharing
    commr_block.var = attach_memory_block(FILENAME_R, COMM_BLOCK_SIZE, commr_block.key);
    
    if (!(datar_block.var && commr_block.var)) {
        storelog("%s", "receiver failed to get shared memory");
        return -1; 
    }    
    
    s_info.maxevents = 1000; // maxevent that will taken from kernel to process buffer
 
    if (create_socket() == -1) { return -1; } // create socket
    if (make_nonblocking(s_info.servsoc_fd) == -1) { return -1; } // make the server socket file descriptor as non blocking
 
    // somaxconn is defined in socket.h
    if (listen(s_info.servsoc_fd, SOMAXCONN) < 0) { // listen for incoming connections
        storelog("%s", "receiver failed to listen on port");
        return -1;
    }
 
    s_info.epoll_fd = epoll_create1(0); // create epoll instance
    if (s_info.epoll_fd == -2) {
        storelog("%s", "receiver failed to create epoll instance");
        return -1;
    }

    if (add_to_list(s_info.servsoc_fd) == -1) { return -1; } // add socket file descriptor to epoll list
    
    if (hcreate_table(&htable, 1000) != 0) {
        storelog("%s", "receiver failed to create fd-capacity table");
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