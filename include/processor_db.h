#ifndef PROCESSOR_DB_H
#define PROCESSOR_DB_H

typedef struct db_statements {
    char statement_name[4];
    char *statement;
    int param_count;
}db_statements;

int close_database_connection();
int connect_to_database();
int store_data_in_database(int, char *, int);
int retrive_data_from_database(char *);
int store_comms_into_database(char *);
int store_commr_into_database(char *);
int retrive_comms_from_database(char *);
int retrive_commr_from_database(char *);



#endif // PROCESSOR_DB_H