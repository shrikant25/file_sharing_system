#ifndef _USR_PRGM_H
#define _USR_PRGM_H

#define pathsize 100
#define confdatasize 200
#define db_conn_command_size 100
#define conn_param_size 30

typedef struct db_statements {
    char *statement_name;
    char *statement;
    int param_count;
}db_statements;


#define statement_count 1

db_statements dbs[statement_count] = {
    {
      .statement_name = "import_file",
      .statement = "INSERT INTO FILE_DATA(file_id, file_name, file_size, file_data) VALUES(GEN_RANDOM_UUID(), $1, $2, lo_import($3));",
      .param_count = 3,
    },
};


int prepare_statements ();
int connect_to_database (char *); 
int import_file (char *);
int export_file (char *);
int send_file (char *);

#endif //_USR_PRGM_H