#ifndef I_SENDER_H
#define I_SENDER_H

#define MESSAGE_SIZE 136

typedef struct server_info {
  unsigned int port;
  unsigned int servsoc_fd;
  unsigned long ipaddress;
  char uuid[37];
  char data[MESSAGE_SIZE];
} server_info;

typedef struct db_statements {
  char *statement_name;
  char *statement;
  int param_count;
} db_statements;


#define statement_count 3

db_statements dbs[statement_count] = {
    { 
      .statement_name = "is_storelog",  
      .statement = "INSERT INTO logs (log) VALUES ($1);",
      .param_count = 1
    },
    { 
      .statement_name = "get_data",  
      .statement = "SELECT si.comssport, si.ipaddress, js.jobid, js.jobdata FROM job_scheduler js JOIN \
                    sysinfo si ON js.jdestination = si.system_name WHERE js.jstate = 'S-5' ORDER BY js.jpriority LIMIT 1;",
      .param_count = 0
    },
    {
      .statement_name = "update_status",
      .statement = "UPDATE job_scheduler SET jstate = (SELECT CASE WHEN $1 > 0 THEN 'C' ELSE 'D') WHERE jobid = $2::uuid;",
      .param_count = 2
    }
};

int connect_to_database (char * _conninfo); 
int prepare_statements();
void store_log(char * _logtext);
int read_data_from_database (server_info * _servinfo);
void run_server();
void update_status(char * _uuid, int __status);

#endif  