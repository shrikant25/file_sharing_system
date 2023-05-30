#ifndef I_RECEIVER_H
#define I_RECEIVER_H

#define MESSAGE_SIZE 147


typedef struct server_info{
    unsigned short int port;
    unsigned int servsoc_fd;
    unsigned long ipaddress;
    
}server_info;

typedef struct db_statements {
    char *statement_name;
    char *statement;
    int param_count;
}db_statements;


#define statement_count 2

db_statements dbs[statement_count] = {
    { 
      .statement_name = "ir_storelog",  
      .statement = "INSERT INTO logs (log) VALUES ($1);",
      .param_count = 1
    },
    { 
      .statement_name = "store_data",  
      .statement = "INSERT INTO job_scheduler(jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority)\
                  VALUES ($2, 'N-1', '0', $1, GEN_RANDOM_UUID(), (SELECT jobid from job_scheduler where jobid = jparent_jobid), 0, 0);",
      .param_count = 2
    }
};

int connect_to_database();
int prepare_statements();
void store_log(char *);
void store_data_in_database(int, char *);

#endif

 