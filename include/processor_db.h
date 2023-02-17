#ifndef PROCESSOR_DB_H
#define PROCESSOR_DB_H


int close_database_connection();
int connect_to_database();
int store_data_in_database(char *);
int retrive_data_from_database(char *);
int store_comms_into_database(char *);
int store_commr_into_database(char *);
int retrive_comms_from_database(char *);
int retrive_commr_from_database(char *);



#endif // PROCESSOR_DB_H