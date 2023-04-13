

INSERT INTO job_scheduler(jobdata, jstate, 
    jtype, jsource, jobid, jparent_jobid, 
    jdestination, jpriority) 
VALUES(
    (select create_message('01'::text, 'helloqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq'::bytea, 'M1'::text, 'M2'::text, '05'::text)),
            'N-1', '0', '0', GEN_RANDOM_UUID(), 
     (select jobid 
     from job_scheduler 
     where jparent_jobid = jobid), '0', '5');


INSERT INTO job_scheduler(jobdata, jstate, 
    jtype, jsource, jobid, jparent_jobid, 
    jdestination, jpriority) 
VALUES(
    (select create_message('01'::text, 'hello'::bytea, 'M2'::text, 'M3'::text, '05'::text)),
     'N-1', '0', '0', GEN_RANDOM_UUID(), 
     (select jobid 
     from job_scheduler 
     where jparent_jobid = jobid), '0', '5');


INSERT INTO job_scheduler(jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority) 
VALUES( 
    (select create_message('01'::text, 'hello'::bytea, 'M2'::text, 'M3'::text, '05'::text)),
     'N-1', '0', '0', GEN_RANDOM_UUID(), 
     (select jobid 
     from job_scheduler 
     where jparent_jobid = jobid), '0', '5');


INSERT INTO job_scheduler(jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority) 
VALUES(
    (select create_message('01'::text, 'helloqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq'::bytea, 'M1'::text, 'M2'::text, '05'::text)),
     'N-1', '0', '0', GEN_RANDOM_UUID(), 
     (select jobid 
     from job_scheduler 
     where jparent_jobid = jobid), '0', '5');

INSERT into systems VALUES('   M2');
INSERT INTO sysinfo VALUES('   M2', 123456, 4000, 150);
INSERT INTO sysinfo VALUES('   M2', 123457, 4001, 155);
INSERT INTO sysinfo VALUES('   M2', 123458, 4002, 149);
