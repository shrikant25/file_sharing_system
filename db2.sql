
CREATE OR REPLACE FUNCTION call_jobs()
RETURNS void AS
$$
BEGIN
    RAISE NOTICE'HURRA';
END;
$$
LANGUAGE'plpgsql';


SELECT cron.schedule('* * * * * *', 'select call_jobs();');




DROP TABLE logs, receivers_comms, receiving_conns, job_scheduler, sysinfo, senders_comms, sending_conns;


CREATE TABLE job_scheduler (jidx SERIAL PRIMARY KEY, 
                        jobdata bytea NOT NULL,
                        jstate CHAR(5) NOT NULL DEFAULT 'N-1',
                        jtype TEXT NOT NULL DEFAULT '1',
                        jsource TEXT NOT NULL,
                        jobid UUID UNIQUE NOT NULL,
                        jparent_jobid UUID REFERENCES job_scheduler(jobid) 
                            ON UPDATE CASCADE ON DELETE CASCADE,
                        jdestination TEXT NOT NULL DEFAULT 0,
                        jpriority  TEXT NOT NULL DEFAULT '10',
                        jcreation_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP);


CREATE TABLE receivers_comms (rcomid SERIAL PRIMARY KEY, 
                              mdata bytea NOT NULL, 
                              mtype INTEGER NOT NULL);

CREATE TABLE receiving_conns (rfd TEXT NOT NULL PRIMARY KEY, 
                              ripaddr TEXT NOT NULL, 
                              rcstatus TEXT NOT NULL,
                              rctime TIMESTAMP DEFAULT CURRENT_TIMESTAMP);



CREATE TABLE sending_conns (sconnid SERIAL PRIMARY KEY, 
                            sfd INTEGER NOT NULL,
                            sipaddr BIGINT NOT NULL, 
                            scstatus SMALLINT NOT NULL,
                            sctime TIMESTAMP DEFAULT CURRENT_TIMESTAMP);

CREATE TABLE senders_comms (scommid SERIAL PRIMARY KEY, 
                            mdata Text NOT NULL, 
                            mtype SMALLINT NOT NULL);



CREATE TABLE logs (logid SERIAL PRIMARY KEY,
                   log TEXT NOT NULL,
                   lgtime TIMESTAMP DEFAULT CURRENT_TIMESTAMP);

CREATE TABLE sysinfo (system_name CHAR(10) PRIMARY key,
                        ipaddress Text NOT NULL,
                        systems_capacity INTEGER NOT NULL);


CREATE OR REPLACE FUNCTION create_message(
    message_type text,
    messaget bytea,
    message_source text,
    message_destination text,
    message_priority text
) RETURNS bytea
AS
$$
DECLARE   
    hnmessage bytea;
    fixed_type text;
    fixed_source text;
    fixed_destination text;
    fixed_priority text;
BEGIN
    fixed_type := lpad(message_type, 5, ' ');
    fixed_source := lpad(message_source, 5, ' ');
    fixed_destination := lpad(message_destination, 5, ' ');
    fixed_priority := lpad(message_priority, 5, ' ');
    
    hnmessage := gen_random_uuid()::text::bytea || fixed_type::bytea || 
                fixed_source::bytea || fixed_destination::bytea || 
                fixed_priority::bytea ||  to_char(now(), 'YYYY-MM-DD HH24:MI:SS.US')::bytea || messaget;
    
    RETURN md5(hnmessage)::bytea || hnmessage;
END;
$$
LANGUAGE 'plpgsql';


INSERT INTO sysinfo VALUES('   M2', '123456', 50);

INSERT INTO job_scheduler
    (jobdata, jstate, jtype, jsource, 
        jobid, jparent_jobid, jdestination, jpriority) 
VALUES('__ROOT__', 'N-0', '0', '   M3', 
    GEN_RANDOM_UUID(), NULL, '   M3', 0);

UPDATE job_scheduler 
SET jparent_jobid = jobid 
WHERE jidx = 1;

ALTER TABLE job_scheduler 
ALTER COLUMN jparent_jobid
SET NOT NULL;

SELECT jidx, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority, jcreation_time FROM job_scheduler;


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




UPDATE job_scheduler 
SET jstate = 
    CASE 
        WHEN jstate = 'N-1' AND encode(substr(jobdata, 1, 32), 'escape') = md5(substr(jobdata, 33)) THEN 'N-2'
        WHEN jstate = 'N-1' AND encode(substr(jobdata, 1, 32), 'escape') != md5(substr(jobdata, 33)) THEN 'D'
        ELSE jstate
    END;

UPDATE job_scheduler 
SET jstate = 'N-3' 
WHERE jstate = 'N-2' 
AND 
encode(substr(jobdata, 79, 5), 'escape') = (SELECT jdestination FROM job_scheduler WHERE jidx= 1);

UPDATE job_scheduler 
SET jstate = 'S-1', 
jdestination = encode(substr(jobdata, 79, 5), 'escape') 
WHERE jstate = 'N-2' 
AND 
encode(substr(jobdata, 79, 5), 'escape') != (SELECT jdestination FROM job_scheduler WHERE jidx= 1);


SELECT jidx, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority, jcreation_time FROM job_scheduler;



UPDATE job_scheduler 
SET jstate = 'N-4', 
jtype = encode(substr(jobdata, 69, 5), 'escape')
WHERE jstate = 'N-3';



UPDATE job_scheduler AS js
SET jstate = (
    SELECT 
        CASE 
            WHEN LENGTH(jobdata) > systems_capacity THEN 'S-2'
            ELSE 'S-3'
        END
    FROM sysinfo
    WHERE system_name = js.jdestination
)
WHERE jstate = 'S-1';



-- UPDATE job_scheduler AS js
-- SET jstate = CASE 
--                 WHEN jstate = 'S-1' AND LENGTH(jobdata) > (SELECT systems_capacity FROM sysinfo WHERE system_name = js.jdestination) THEN 'S-2'
--                 WHEN jstate = 'S-1' AND LENGTH(jobdata) <= (SELECT systems_capacity FROM sysinfo WHERE system_name = js.jdestination) THEN 'S-3'
--                 ELSE jstate
--             END;




-- UPDATE job_scheduler AS js
-- SET jstate = 'S-2', 
-- jdestination = (SELECT ipaddress 
--                 FROM sysinfo 
--                 WHERE system_name = js.jdestination) 
-- WHERE jstate = 'S-1' 
-- AND 
-- LENGTH(jobdata) > (SELECT systems_capacity 
--                     FROM sysinfo 
--                     WHERE system_name = js.jdestination);



-- UPDATE job_scheduler AS js
-- SET jstate = 'S-3', 
-- jdestination = (SELECT ipaddress 
--                 FROM sysinfo 
--                 WHERE system_name = js.jdestination) 
-- WHERE jstate = 'S-1' 
-- AND 
-- LENGTH(jobdata) <= (SELECT systems_capacity 
--                     FROM sysinfo 
--                     WHERE system_name = js.jdestination);


UPDATE job_scheduler 
SET jdestination = (SELECT ipaddress 
                    FROM sysinfo 
                    WHERE system_name = jdestination) 
WHERE jstate IN('S-3', 'S-2');


SELECT jidx, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority, jcreation_time FROM job_scheduler;

--take unique ip from all messages at s-3 stage, insert into sending_conns with status closed, create a message for sender to open location,
--if already present and status = closed, create a message for sender to open location

-- INSERT into senders_comms (mdata, mtype) 
-- SELECT array_to_string(array_agg(jdestination::text),','), '1'  
-- FROM job_scheduler 
-- WHERE jstate = 'S-3' 
-- AND jdestination 
-- NOT IN 
-- (SELECT sipaddr FROM sending_conns);   




SELECT jidx, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority, jcreation_time FROM job_scheduler;





WITH cte_jobdata AS (

    SELECT 
        jobid, 
        jdestination, 
        encode(substr(job_scheduler.jobdata, 74, 5), 'escape') as jsource,
        jpriority,
        seqnum, 
        substring(jobdata, (seqnum * systems_capacity +1)::int, systems_capacity) AS subdata
    
    FROM job_scheduler
    JOIN sysinfo 
    ON encode(substr(job_scheduler.jobdata, 79, 5), 'escape') = sysinfo.system_name
    CROSS JOIN generate_series(0, ceil(length(jobdata)::decimal/ systems_capacity)-1) AS seqnum
    WHERE jstate = 'S-2'

), 
cte_msg AS (
    SELECT  
            create_message('2'::text, 
                lpad(seqnum::text, 8, ' ')::bytea || jobid::text::bytea || lpad(length(subdata)::text, 10, ' ')::bytea || subdata, 
                btrim(jsource, ' '), 
                btrim(jdestination, ' '),
                jpriority::text) AS msg,
            jobid, 
            jsource,
            jdestination, 
            jpriority, 
            seqnum
    FROM cte_jobdata
)
INSERT INTO job_scheduler (jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority)
SELECT msg, 'S-3', '2', jsource, encode(substr(msg, 33, 36), 'escape')::uuid, jobid, jdestination, jpriority 
FROM cte_msg;




WITH cte_msginfo as(

    SELECT
        create_message(
                '3'::text,
                jobid::text::bytea || lpad(length(jobdata)::text, 10, ' ')::bytea || lpad((ceil(length(jobdata)::decimal/ systems_capacity))::text, 10, ' ')::bytea,
                btrim(encode(substr(job_scheduler.jobdata, 74, 5), 'escape'), ' '), 
                btrim(jdestination, ' '),
                jpriority::text
        )as mdata, 
        jobid, 
        jdestination, 
        encode(substr(job_scheduler.jobdata, 74, 5), 'escape') as jsource,
        jpriority
        FROM job_scheduler
    JOIN sysinfo 
    ON encode(substr(job_scheduler.jobdata, 79, 5), 'escape') = sysinfo.system_name
    WHERE jstate = 'S-2'
)
INSERT INTO job_scheduler (jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority)
SELECT mdata, 'S-3', '3', jsource, encode(substr(mdata, 33, 36), 'escape')::uuid, jobid, jdestination, jpriority 
FROM cte_msginfo; 
        

UPDATE job_scheduler SET jstate = 'S-2W' WHERE jstate = 'S-2';






SELECT jidx, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority, jcreation_time FROM job_scheduler;



SELECT                           
    encode(substr(jobdata, 115, 8), 'escape') AS t1,
    encode(substr(jobdata, 123, 36), 'escape') AS t2,
    encode(substr(jobdata, 159, 10), 'escape') AS t3,
    encode(substr(jobdata, 169), 'escape') AS mdata
FROM job_scheduler;



SELECT                           
    encode(substr(jobdata, 1, 32), 'escape') AS mhash,
    encode(substr(jobdata, 33, 36),'escape') AS uuid,
    encode(substr(jobdata, 69, 5), 'escape') AS message_type,
    encode(substr(jobdata, 74, 5), 'escape') AS message_source,
    encode(substr(jobdata, 79, 5), 'escape') AS message_destination,
    encode(substr(jobdata, 84, 5), 'escape') AS message_priority, encode(substr(jobdata, 89, 26), 'escape') as tm
FROM job_scheduler;


SELECT                           
    encode(substr(jobdata, 1, 32), 'escape') AS mhash,
    encode(substr(jobdata, 33, 36),'escape') AS uuid,
    encode(substr(jobdata, 69, 5), 'escape') AS message_type,
    encode(substr(jobdata, 74, 5), 'escape') AS message_source,
    encode(substr(jobdata, 79, 5), 'escape') AS message_destination,
    encode(substr(jobdata, 84, 5), 'escape') AS message_priority, 
    encode(substr(jobdata, 89, 26), 'escape') AS tm,
    encode(substr(jobdata, 115, 36), 'escape') AS parent_job_id,
    encode(substr(jobdata, 151, 10), 'escape') AS data_length,
    encode(substr(jobdata, 161, 10), 'escape') AS chunk_count
FROM job_scheduler WHERE jtype = '3';
















INSERT INTO senders_comms (mdata, mtype)
SELECT DISTINCT string_agg(DISTINCT js.jdestination::text, ','), 1
FROM job_scheduler js
WHERE js.jstate = 'S-3'
AND NOT EXISTS (
  SELECT 1
  FROM sending_conns sc
  WHERE sc.sipaddr::text = js.jdestination
);

INSERT into sending_conns (sfd, sipaddr, scstatus) 
SELECT DISTINCT -1, jdestination::BIGINT, 1  
FROM job_scheduler js 
WHERE js.jstate = 'S-3' 
AND NOT EXISTS (
  SELECT 1 
  FROM sending_conns sc
  WHERE sc.sipaddr::text = js.jdestination
);   


select *  from senders_comms;
select * from sending_conns;


UPDATE sending_conns 
SET sfd = ($1), scstatus = ($2) 
WHERE scipddr = ($3);

SELECT jobdata 
FROM job_scheduler js 
WHERE js.jstate = 'S-3'
AND 
EXISTS (SELECT  1 
        FROM sending_conns sc 
        WHERE sc.sipaddr::text = js.jdestination 
        AND sc.scstatus = 2)
    ORDER BY jpriority DESC
LIMIT 1;







SELECT                           
    encode(substr(jobdata, 115, 8), 'escape') AS t1,
    encode(substr(jobdata, 123, 36), 'escape') AS t2,
    encode(substr(jobdata, 159, 10), 'escape') AS t3,
    encode(substr(jobdata, 169), 'escape') AS mdata
FROM job_scheduler;



SELECT                           
    encode(substr(jobdata, 1, 32), 'escape') AS mhash,
    encode(substr(jobdata, 33, 36),'escape') AS uuid,
    encode(substr(jobdata, 69, 5), 'escape') AS message_type,
    encode(substr(jobdata, 74, 5), 'escape') AS message_source,
    encode(substr(jobdata, 79, 5), 'escape') AS message_destination,
    encode(substr(jobdata, 84, 5), 'escape') AS message_priority, encode(substr(jobdata, 89, 26), 'escape') as tm
FROM job_scheduler;



SELECT                           
    encode(substr(jobdata, 1, 32), 'escape') AS mhash,
    encode(substr(jobdata, 33, 36),'escape') AS uuid,
    encode(substr(jobdata, 69, 5), 'escape') AS message_type,
    encode(substr(jobdata, 74, 5), 'escape') AS message_source,
    encode(substr(jobdata, 79, 5), 'escape') AS message_destination,
    encode(substr(jobdata, 84, 5), 'escape') AS message_priority, 
    encode(substr(jobdata, 89, 26), 'escape') AS tm,
    encode(substr(jobdata, 115, 36), 'escape') AS parent_job_id,
    encode(substr(jobdata, 151, 10), 'escape') AS data_length,
    encode(substr(jobdata, 161, 10), 'escape') AS chunk_count
FROM job_scheduler WHERE jtype = '3';






--encode(substr(jobdata, 115), 'escape') AS message








SELECT jidx, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority, jcreation_time FROM job_scheduler;


SELECT jidx, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority, jcreation_time FROM job_scheduler;

SELECT jidx, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority, jcreation_time FROM job_scheduler;

-- UPDATE job_scheduler 
-- SET jstate = 
--     CASE 
--         WHEN jstate = 'N-1' AND encode(substr(jobdata, 1, 32), 'escape') = md5(substr(jobdata, 33)) THEN 'N-2'
--         WHEN jstate = 'N-1' AND encode(substr(jobdata, 1, 32), 'escape') != md5(substr(jobdata, 33)) THEN 'D'
--         WHEN jstate = 'N-2' AND encode(substr(jobdata, 79, 5), 'escape') = (SELECT jsource FROM job_scheduler WHERE jidx= 1) THEN 'N-3'
--         WHEN jstate = 'N-2' AND encode(substr(jobdata, 79, 5), 'escape') != (SELECT jsource FROM job_scheduler WHERE jidx= 1) THEN 'S-1'
--         ELSE jstate
--     END;




SELECT                           
    encode(substr(jobdata, 1, 32),'escape') AS hash,
    encode(substr(jobdata, 33, 36),'escape') AS uuid,
    encode(substr(jobdata, 69, 5), 'escape') AS message_type,
    encode(substr(jobdata, 75, 5), 'escape') AS message_source,
    encode(substr(jobdata, 79, 5), 'escape') AS message_destination,
    encode(substr(jobdata, 84, 5),'escape') AS message_priority,
    encode(substr(jobdata, 89, 26), 'escape') AS timestamp,
    --encode(substr(jobdata, 115), 'escape') AS message
FROM job_scheduler;




















-- -- in c query
-- INSERT INTO job_scheduler(jobdata, jstate, jtype, jsource_ip, jobid, jparent_jobid, jdestination_ip, jpriority) VALUES($2, 'N-1', 0, $1, GEN_RANDOM_UUID(), (select jobid from job_scheduler where jidx = 1), 0, 0);

-- delete from cron.job_run_details;
-- select * from cron.job_run_details;

-- delete from cron.job where jobid = 1;


-- -- inside postgresql.conf

-- shared_preload_libraries='pg_cron' 
-- cron.database_name='shrikant'

-- -- inside pg_hba.conf
-- # "local" is for Unix domain socket connections only
-- local   all             all                                     peer
-- # IPv4 local connections:
-- host    all             all             127.0.0.1/32            trust
-- # IPv6 local connections:
-- host    all             all             ::1/128                 trust














