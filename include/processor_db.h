#ifndef PROCESSOR_DB_H
#define PROCESSOR_DB_H

#include <libpq-fe.h>

int close_database_connection();
int connect_to_database();
int store_data_in_database(char *);
int retrive_data_from_database(char *);
int store_comms_into_database(char *);
int store_commr_into_database(char *);
int retrive_comms_from_database(char *);
int retrive_commr_from_database(char *);


typedef struct database_info{
    
    PGconn *connection;
    unsigned int statement_count = 8;
    
    char *statement_names[statement_count] = {
                                "s0" // store data in database ;
                                "s1" // retrive data from_database;
                                "s2" // store (commr received from receiver) into database;
                                "s3" // store (comms received from senders) into database;
                                "s4" // retrive commr from database intended for receiver;
                                "s5" // retrive comms from database intended for sender;
                                "s6" // update for_sender table set status as 2 i.e "connection establishment in progress";
                                "s7" // update for_sender table set status as 3 i.e "connection established"
                              };


    char statments[statement_count] = {
                        "INSERT INTO raw_data (data) values($1)";
                        "SELECT fd, data FROM send_data limit 1";
                        "INSERT INTO open_connections (fd, ipaddr) VALUES($1, $2)";
                        "INSERT INTO  senders_comm (msgid, status) VALUES($1, $2)";
                        "SELECT fd FROM for_receiver where status = 1 limit 1";
                        "SELECT cid, ipaddr FROM for_sender where status = 1 limit 1";
                        "UPDATE for_sender set status = 2 where cid = ($1)";
                        "UPDATE for_sender set status = 3 where cid = ($1)";
                        }; 

    int param_count[statement_count] = { 1, 0, 2, 2, 0, 0, 1, 1};

}database_info;


#endif // PROCESSOR_DB_H