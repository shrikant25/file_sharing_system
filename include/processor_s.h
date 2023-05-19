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
    char *statement_name;
    char *statement;
    int param_count;
}db_statements;


#define statement_count 7

db_statements dbs[statement_count] = {
    { 
      .statement_name = "s_get_info", 
      .statement = "SELECT sc.sfd, js.jobid FROM job_scheduler js JOIN sysinfo si ON js.jdestination = si.system_name \
                    JOIN sending_conns sc ON sc.sipaddr = si.ipaddress WHERE sc.scstatus = 2 AND js.jstate = 'S-4' \
                    ORDER BY js.jpriority DESC LIMIT 1;",
      .param_count = 0,
    },
    { 
      .statement_name = "s_update_conns", 
      .statement = "UPDATE sending_conns SET scstatus = ($3), sfd = ($2)  WHERE sipaddr = ($1);",
      .param_count = 3,
    },
    { 
      .statement_name = "s_update_job_status2", 
      .statement = "UPDATE job_scheduler SET jstate = (SELECT CASE WHEN ($2) > 0 THEN 'C' ELSE 'D' END) WHERE jobid = $1::uuid;",
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
    },
    {
      .statement_name = "s_get_data",
      .statement = " SELECT (js.jobdata|| lo_get(f.file_data, js.data_offset * (si.system_capacity-168), si.system_capacity-168) \
                || (lpad('',si.system_capacity-length(js.jobdata||\
        lo_get(f.file_data,js.data_offset*(si.system_capacity-168), si.system_capacity-168)), ' '))::bytea) FROM job_scheduler \
        js JOIN job_scheduler js2 ON js.jparent_jobid = js2.jobid \
       JOIN files f ON f.file_name = encode(js2.jobdata, 'escape') \
       JOIN sysinfo si ON  js.jdestination = si.system_name JOIN sending_conns sc ON sc.sipaddr = si.ipaddress WHERE js.jobid = $1::uuid;",
      .param_count = 1
    }
};

#endif // PROCESSOR_H 

