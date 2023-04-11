

INSERT INTO job_scheduler(jobdata, jstate, 
    jtype, jsource, jobid, jparent_jobid, 
    jdestination, jpriority) 
VALUES(
    (select create_message('01'::text, 'hello'::bytea, 'M1'::text, 'M2'::text, '05'::text)),
            'N-1', '0', '0', GEN_RANDOM_UUID(), 
     (select jobid 
     from job_scheduler 
     where jidx = 1), '0', '5');


INSERT INTO job_scheduler(jobdata, jstate, 
    jtype, jsource, jobid, jparent_jobid, 
    jdestination, jpriority) 
VALUES(
    (select create_message('01'::text, 'hello'::bytea, 'M2'::text, 'M3'::text, '05'::text)),
     'N-1', '0', '0', GEN_RANDOM_UUID(), 
     (select jobid 
     from job_scheduler 
     where jidx = 1), '0', '5');


INSERT INTO job_scheduler(jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority) 
VALUES( 
    (select create_message('01'::text, 'hello'::bytea, 'M2'::text, 'M3'::text, '05'::text)),
     'N-1', '0', '0', GEN_RANDOM_UUID(), 
     (select jobid 
     from job_scheduler 
     where jidx = 1), '0', '5');


INSERT INTO job_scheduler(jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority) 
VALUES(
    (select create_message('01'::text, 'hello'::bytea, 'M1'::text, 'M2'::text, '05'::text)),
     'N-1', '0', '0', GEN_RANDOM_UUID(), 
     (select jobid 
     from job_scheduler 
     where jidx = 1), '0', '5');
