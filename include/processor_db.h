#ifndef PROCESSOR_DB_H
#define PROCESSOR_DB_H

#include <libpq-fe.h>

int close_database_connection();
int connect_to_database();
int store_data_in_database(char *);
int retrive_data_from_database(char *);
int retrive_comms_from_database(char *);
int retrive_commr_from_database(char *);
int store_comms_into_database(char *);
int store_commr_into_database(char *);

typedef struct database_info{
    
    PGconn *connection;
    unsigned int statement_count = 6;
    
    char *statement_names[statement_count] = {
                                "s0" // store data in database ;
                                "s1" // retrive data from_database;
                                "s2" // store (comms received from receiver) into database;
                                "s3" // store (comms received senders comm) into database;
                                "s4" // retrive comms from database intended for sender;
                                "s5" // retrive comms from database intended for receiver;
                              };


    char statments[statement_count] = {
                        "INSERT INTO raw_data (data) values($1)";
                        "SELECT FD, DATA FROM send_data limit 10";
                        "INSERT INTO open_connections (fd, ipaddr) VALUES($1, $2)";
                        "INSERT INTO  senders_comm (msgid, status) VALUES($1, $2)";
                        "SELECT FD FROM for_sender";
                        "SELECT FD FROM for_reciever";
                        }; 

    int param_count[statement_count] = { 1, 0, 2, 2, 0, 0};

}database_info;


#endif // PROCESSOR_DB_H