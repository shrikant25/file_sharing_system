#ifndef PROCESSOR_R_H
#define PROCESSOR_R_H

#include "partition.h"

void store_log(char *);
int run_process();
int retrive_commr_from_database(char *);
int store_data_in_database(newmsg_data *);
int store_commr_into_database(receivers_message *);
int get_data_from_receiver();
int send_message_to_receiver();
int get_message_from_receiver();

typedef struct db_statements {
    char statement_name[20];
    char *statement;
    int param_count;
}db_statements;

int connect_to_database();
int prepare_statements();

#define statement_count 3

db_statements dbs[statement_count] = {
    { 
      .statement_name = "r_insert_data",  
      .statement = "INSERT INTO job_scheduler(jobdata, jstate, jtype, \
                    jsource, jobid, jparent_jobid, jdestination, \
                    jpriority) VALUES($2, 'N-0', '0', $1, GEN_RANDOM_UUID(), \
                    (select jobid from job_scheduler where jparent_jobid = jobid), 0, 0);",
      .param_count = 2,
    },
    { 
      .statement_name = "r_insert_conns", 
      .statement = "INSERT INTO receiving_conns (rfd, ripaddr, rcstatus) \
                    VALUES ($1, $2, $3) ON CONFLICT (rfd) DO UPDATE SET rcstatus = ($3);",
      .param_count = 3,
    },
    {
      .statement_name = "pr_storelogs", 
      .statement = "INSERT INTO logs (log) VALUES ($1);",
      .param_count = 1,
    }
};

#endif // PROCESSOR_H 