#ifndef PROCESSOR_R_H
#define PROCESSOR_R_H

#include <semaphore.h>
#include "partition.h"
#include "connect_and_prepare.h"

void store_log(char *);
int run_process();
int retrive_commr_from_database(char *);
int store_data_in_database(newmsg_data *);
int store_commr_into_database(receivers_message *);
int get_data_from_receiver();
int send_message_to_receiver();
int get_message_from_receiver();


typedef struct datablocks {
    char *datar_block;
    char *commr_block;
}datablocks;

typedef struct semlocks {
    sem_t *sem_lock_datar;
    sem_t *sem_lock_commr;
    sem_t *sem_lock_sigr;
}semlocks;


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