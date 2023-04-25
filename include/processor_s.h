#ifndef PROCESSOR_S_H
#define PROCESSOR_S_H

#include "partition.h"

int run_process();
int communicate_with_sender();
int give_data_to_sender();
void store_log(char *);
int retrive_comms_from_database(char *);
int store_comms_into_database(char *);
int retrive_data_from_database(char *); 
int prepare_statements();
int connect_to_database();

typedef struct db_statements {
    char statement_name[20];
    char *statement;
    int param_count;
}db_statements;


#define statement_count 6

db_statements dbs[statement_count] = {
    { 
      .statement_name = "s_get_data", 
      .statement = "SELECT sc.sfd, js.jobid, js.jobdata\
                    FROM job_scheduler js, sending_conns sc \
                    WHERE js.jstate = 'S-3' \
                    AND sc.sipaddr::text = js.jdestination \
                    AND sc.scstatus = 2 \
                    ORDER BY jpriority DESC LIMIT 1;",
      .param_count = 0,
    },
    { 
      .statement_name = "s_update_conns", 
      .statement = "UPDATE sending_conns SET scstatus = ($3), sfd = ($2)  WHERE sipaddr = ($1);",
      .param_count = 3,
    },
    { 
      .statement_name = "s_update_job_status2", 
      .statement = "UPDATE job_scheduler \
                    SET jstate = (\
                        SELECT\
                            CASE\
                                WHEN ($2) > 0 THEN 'C'\
                                ELSE 'D'\
                            END\
                        )\
                    WHERE jobid = $1::uuid;",
      .param_count = 2,
    },
    { 
      .statement_name = "s_get_comms", 
      .statement = "WITH sdata AS(SELECT scommid FROM senders_comms WHERE mtype IN(1, 2) LIMIT 1) \
                    DELETE FROM senders_comms WHERE scommid = (SELECT scommid FROM sdata) RETURNING mtype, mdata1, mdata2;",
      .param_count = 0,
    },
    {
      .statement_name = "s_update_job_status1", 
      .statement = "UPDATE job_scheduler SET jstate = jstate || 'W' WHERE jobid = $1::uuid;",
      .param_count = 1, 
    },
    {
      .statement_name = "ps_storelogs", 
      .statement = "INSERT INTO logs (log) VALUES ($1);",
      .param_count = 1,
    }

};

#endif // PROCESSOR_H 