#ifndef MESSAGE_H
#define MESSAGE_H

#define message_size 1024 * 128

typedef struct receivers_message {
    unsigned int fd;
    unsigned int ipaddr;
    unsigned int status;
}rconmsg;

typedef struct receivers_data{
    unsigned int fd;
    unsigned char data[message_size];
}rcondata;

#endif //_message_h