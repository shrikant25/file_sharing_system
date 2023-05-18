#ifndef PARTN_H
#define PARTN_H

#define MESSAGE_SIZE (1024 * 128)

// todo - increase size of partitions to accomodate some meta info
#define TOTAL_PARTITIONS 80 // alway keep it in multiples of 
#define DPARTITION_SIZE (MESSAGE_SIZE + 42)
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

typedef struct capacity_info {
    char ipaddress[17];
    unsigned int capacity;
}capacity_info;

typedef struct receivers_message {
    unsigned int fd;
    unsigned int ipaddr;
    unsigned int status;
}receivers_message;

typedef struct newmsg_data {
    unsigned int data1;
    unsigned int data2;
    char data[MESSAGE_SIZE];
}newmsg_data;

typedef struct open_connection {
    unsigned char type;
    unsigned int port;
    unsigned int ipaddress;
}open_connection;

typedef struct close_connection {
    unsigned char type;
    unsigned int fd;
    unsigned int ipaddress;
}close_connection;

typedef struct connection_status {
    unsigned char type;
    unsigned int fd;
    unsigned int ipaddress; 
}connection_status;

typedef struct send_message {
    unsigned char type;
    unsigned int fd;
    char uuid[37];
    char data[MESSAGE_SIZE];
}send_message;

typedef struct message_status {
    unsigned char type;
    unsigned char status;
    char uuid[37];
}message_status;


int get_subblock(char *, int, int);
int get_subblock2(char *, int, int);
void toggle_bit(int, char *, int);
void set_all_bits(char *, int); 
void unset_all_bits(char *, int); 

#endif // PARTN_H
