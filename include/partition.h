#ifndef PARTN_H
#define PARTN_H

#define MESSAGE_SIZE (1024 * 128)

// todo - increase size of partitions to accomodate some meta info
#define TOTAL_PARTITIONS 80
#define DPARTITION_SIZE (MESSAGE_SIZE + 4)
#define DATA_BLOCK_SIZE (DPARTITION_SIZE * TOTAL_PARTITIONS + 10)
#define CPARTITION_SIZE 15
#define COMM_BLOCK_SIZE (CPARTITION_SIZE * TOTAL_PARTITIONS + 10)

// (1024 * 128 + 4) * 80 + 10
// 10 bytes extra for bitmap   __
//                            ('')
//                          _/|__|\_
//                            |  |                        
// 1024 * 64 * 40 + 5
// 1024 * 64 * 40 + 5 

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

typedef struct senders_message {
    unsigned int type;
    unsigned int data1;
    unsigned int data2;
}senders_message;


int get_subblock(char *, int, int);
int get_subblock2(char *, int, int);
void toggle_bit(int, char *, int);
void set_all_bits(char *, int); 
void unset_all_bits(char *, int); 

#endif // PARTN_H
