#ifndef _USR_PRGM_H
#define _USR_PRGM_H

#define pathsize 100
#define confdatasize 200
#define db_conn_command_size 100
#define conn_param_size 30

typedef struct db_statements {
    char statement_name[20];
    char *statement;
    int param_count;
}db_statements;


#define statement_count 1

db_statements dbs[statement_count] = {
    { 
      .statement_name = "u_storelog",  
      .statement = "INSERT INTO logs (log) VALUES ($1);",
      .param_count = 1
    }
};



#endif //_USR_PRGM_H