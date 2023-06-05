#ifndef PARTN_H
#define PARTN_H

#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/shm.h>

#define MESSAGE_SIZE (1024 * 128)

// todo - increase size of partitions to accomodate some meta info
#define TOTAL_PARTITIONS 80 // alway keep it in multiples of 
#define DPARTITION_SIZE (MESSAGE_SIZE + 46)
#define DATA_BLOCK_SIZE (DPARTITION_SIZE * TOTAL_PARTITIONS + 10)
#define CPARTITION_SIZE 20
#define COMM_BLOCK_SIZE (CPARTITION_SIZE * TOTAL_PARTITIONS + 10)

// (1024 * 128 + 4) * 80 + 10
// 10 bytes extra for bitmap   __
//                            ('')
//                          _/|__|\_
//                            |  |                        
// 1024 * 64 * 40 + 5
// 1024 * 64 * 40 + 5 

extern PGconn *connection; 

typedef struct capacity_info {
    unsigned int capacity;
    unsigned char ipaddress[17];
}capacity_info;

typedef struct receivers_message {
    unsigned int fd;
    unsigned int ipaddr;
    unsigned int status;
}receivers_message;

typedef struct newmsg_data {
    unsigned int data1;
    unsigned int data2;
    unsigned char data[MESSAGE_SIZE];
}newmsg_data;

typedef struct open_connection {
    int type;
    unsigned int port;
    unsigned int ipaddress;
    int scommid;
}open_connection;

typedef struct close_connection {
    int type;
    unsigned int fd;
    unsigned int ipaddress;
    int scommid;
}close_connection;

typedef struct connection_status {
    int type;
    int fd;
    unsigned int ipaddress; 
    int scommid;
}connection_status;

typedef struct send_message {
    unsigned int size;
    unsigned int fd;
    unsigned char uuid[37];
    unsigned char data[MESSAGE_SIZE];
}send_message;

typedef struct message_status {
    int type;
    int status;
    unsigned char uuid[37];
}message_status;

typedef struct semlocks {
  char key[20];
  sem_t *var;
}semlocks;


int get_subblock(char *, int, int);
int get_subblock2(char *, int, int);
void toggle_bit(int, char *, int);
void set_all_bits(char *, int); 
void unset_all_bits(char *, int); 

#endif // PARTN_H
