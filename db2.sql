
CREATE OR REPLACE FUNCTION call_jobs()
RETURNS void AS
$$
BEGIN
    RAISE NOTICE'HURRA';
END;
$$
LANGUAGE'plpgsql';


SELECT cron.schedule('* * * * * *', 'select call_jobs();');

DROP TABLE receivers_comms, receiving_conns, transactions, job_scheduler, sysinfo;

CREATE TABLE receivers_comms (rcomid SERIAL PRIMARY KEY, 
                              mdata bytea NOT NULL, 
                              mtype INTEGER NOT NULL);

CREATE TABLE receiving_conns (rfd TEXT NOT NULL PRIMARY KEY, 
                              ripaddr TEXT NOT NULL, 
                              rcstatus TEXT NOT NULL,
                              rctime TIMESTAMP DEFAULT CURRENT_TIMESTAMP);

CREATE TABLE sysinfo (system_name CHAR(10) PRIMARY key,
                        ipaddress BIGINT NOT NULL,
                        systems_capacity INTEGER NOT NULL);

CREATE TABLE transactions (transactionid INTEGER PRIMARY KEY,
                    transaction_text TEXT);






CREATE TABLE senders_comms (scommid SERIAL PRIMARY KEY, 
                            mdata TEXT NOT NULL, 
                            mtype SMALLINT NOT NULL);

CREATE TABLE sending_conns (sconnid SERIAL PRIMARY KEY, 
                            sfd INTEGER NOT NULL,
                            sipaddr BIGINT NOT NULL, 
                            scstatus SMALLINT NOT NULL,
                            sctime TIMESTAMP DEFAULT CURRENT_TIMESTAMP);

CREATE TABLE logs (logid SERIAL PRIMARY KEY,
                  log TEXT NOT NULL,
                   lgtime TIMESTAMP DEFAULT CURRENT_TIMESTAMP);







DROP TABLE job_scheduler, sysinfo;


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
    
    hnmessage := gen_random_uuid()::text::bytea || fixed_type::bytea || fixed_source::bytea || fixed_destination::bytea || fixed_priority::bytea ||  to_char(now(), 'YYYY-MM-DD HH24:MI:SS.US')::bytea || messaget;
    
    RETURN md5(hnmessage)::bytea || hnmessage;
END;
$$
LANGUAGE 'plpgsql';



CREATE TABLE job_scheduler (jidx SERIAL PRIMARY KEY, 
                        jobdata bytea NOT NULL,
                        jstate CHAR(5) NOT NULL DEFAULT 'N-1',
                        jtype SMALLINT NOT NULL DEFAULT 1,
                        jsource TEXT NOT NULL,
                        jobid UUID UNIQUE NOT NULL,
                        jparent_jobid UUID REFERENCES job_scheduler(jobid) ON UPDATE CASCADE ON DELETE CASCADE,
                        jdestination TEXT NOT NULL DEFAULT 0,
                        jpriority  TEXT NOT NULL DEFAULT 10,
                        jcreation_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP);


INSERT INTO job_scheduler
(jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority) 
VALUES('__ROOT__', 'N-0', 0, '   M3', GEN_RANDOM_UUID(), NULL, '   M3', 0);

UPDATE job_scheduler 
SET jparent_jobid = jobid 
WHERE jidx = 1;

ALTER TABLE job_scheduler 
ALTER COLUMN jparent_jobid
SET NOT NULL;

INSERT INTO job_scheduler(jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority) 
VALUES(
    (select create_message('01'::text, 'hello'::bytea, 'M1'::text, 'M2'::text, '05'::text)),
     'N-1', 0, '0', GEN_RANDOM_UUID(), (select jobid from job_scheduler where jidx = 1), '0', 5);


INSERT INTO job_scheduler(jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority) 
VALUES(
    (select create_message('01'::text, 'hello'::bytea, 'M2'::text, 'M3'::text, '05'::text)),
     'N-1', 0, '0', GEN_RANDOM_UUID(), (select jobid from job_scheduler where jidx = 1), '0', 5);


INSERT INTO job_scheduler(jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority) 
VALUES( 
    (select create_message('01'::text, 'hello'::bytea, 'M2'::text, 'M3'::text, '05'::text)),
     'N-1', 0, '0', GEN_RANDOM_UUID(), (select jobid from job_scheduler where jidx = 1), '0', 5);


INSERT INTO job_scheduler(jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority) 
VALUES(
    (select create_message('01'::text, 'hello'::bytea, 'M1'::text, 'M2'::text, '05'::text)),
     'N-1', 0, '0', GEN_RANDOM_UUID(), (select jobid from job_scheduler where jidx = 1), '0', 5);

CREATE TABLE sysinfo (system_name CHAR(10) PRIMARY key,
                        ipaddress BIGINT NOT NULL,
                        systems_capacity INTEGER NOT NULL);

INSERT INTO sysinfo VALUES('   M2', '123456', 50);



UPDATE job_scheduler 
SET jstate = 
    CASE 
        WHEN jstate = 'N-1' AND encode(substr(jobdata, 1, 32), 'escape') = md5(substr(jobdata, 33)) THEN 'N-2'
        WHEN jstate = 'N-1' AND encode(substr(jobdata, 1, 32), 'escape') != md5(substr(jobdata, 33)) THEN 'D'
        ELSE jstate
    END;

UPDATE job_scheduler SET jstate = 'N-3' where jstate = 'N-2' AND encode(substr(jobdata, 79, 5), 'escape') = (SELECT jdestination FROM job_scheduler WHERE jidx= 1);
UPDATE job_scheduler SET jstate = 'S-1', jdestination = encode(substr(jobdata, 79, 5), 'escape') where jstate = 'N-2' AND encode(substr(jobdata, 79, 5), 'escape') != (SELECT jdestination FROM job_scheduler WHERE jidx= 1);



UPDATE job_scheduler set jstate = 'N-4', jtype = encode(substr(jobdata, 69, 5), 'escape')::SMALLINT where jstate = 'N-3';
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

UPDATE job_scheduler SET jdestination = (SELECT ipaddress FROM sysinfo WHERE system_name = jdestination) WHERE jstate IN('S-3', 'S-2');








SELECT jidx, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority, jcreation_time FROM job_scheduler;










UPDATE job_scheduler AS js
SET jstate = 'S-2', jdestination = (SELECT ipaddress FROM sysinfo WHERE system_name = js.jdestination) 
WHERE jstate = 'S-1' AND LENGTH(jobdata) > (SELECT systems_capacity FROM sysinfo WHERE system_name = js.jdestination);

UPDATE job_scheduler AS js
SET jstate = 'S-3', jdestination = (SELECT ipaddress FROM sysinfo WHERE system_name = js.jdestination) 
WHERE jstate = 'S-1' AND LENGTH(jobdata) <= (SELECT systems_capacity FROM sysinfo WHERE system_name = js.jdestination);




SELECT jidx, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority, jcreation_time FROM job_scheduler;






WITH cte_jobdata AS (
    SELECT 
        jobid, 
        jdestination, 
        encode(substr(job_scheduler.jobdata, 75, 5), 'escape') as jsource,
        jpriority,
        seqnum, length(jobdata), systems_capacity,
        substring(jobdata, seqnum * systems_capacity, systems_capacity) AS subdata
    FROM job_scheduler
    JOIN sysinfo ON encode(substr(job_scheduler.jobdata, 79, 5), 'escape') = sysinfo.system_name
    CROSS JOIN generate_series(0, length(jobdata) / systems_capacity) AS seqnum
    WHERE jstate = 'S-2'
), cte_sent_msg AS (
    SELECT 
        create_message('2'::text, 
                        lpad(seqnum::text, 8, '0')::bytea || jobid::text::bytea || length(subdata)::int::text::bytea || subdata, 
                        jsource::text, 
                        jdestination, 
                        jpriority::text) AS sent_msg,
        jobid, jsource,jdestination, jpriority, seqnum
    FROM cte_jobdata
)
INSERT INTO job_scheduler (jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority)
SELECT 
    sent_msg, 'S-3', 2,   jsource, 
                        encode(substr(sent_msg, 33, 36), 'escape')::uuid, jobid, jdestination, jpriority 
FROM cte_sent_msg;

SELECT                           
    encode(substr(jobdata, 1, 32),'escape') AS hash,
    encode(substr(jobdata, 33, 36),'escape') AS uuid,
    encode(substr(jobdata, 69, 5), 'escape') AS message_type,
    encode(substr(jobdata, 75, 5), 'escape') AS message_source,
    encode(substr(jobdata, 79, 5), 'escape') AS message_destination,
    encode(substr(jobdata, 84, 5),'escape') AS message_priority,
    encode(substr(jobdata, 89, 26), 'escape') AS tstamp,
    encode(substr(jobdata, 115, 8), 'escape') AS t1,
    encode(substr(jobdata, 123, 36), 'escape') AS t2,
    encode(substr(jobdata, 159, 5), 'escape') AS t2,
FROM job_scheduler;

--encode(substr(jobdata, 115), 'escape') AS message



--UPDATE job_scheduler set jobdata = substr(jobdata, 2) where jidx > 1 and jidx%2 != 0;
SELECT                           
    encode(substr(jobdata, 33, 36),'escape') AS uuid,
    encode(substr(jobdata, 69, 5), 'escape') AS message_type,
    encode(substr(jobdata, 75, 5), 'escape') AS message_source,
    encode(substr(jobdata, 79, 5), 'escape') AS message_destination,
    encode(substr(jobdata, 84, 5),'escape') AS message_priority,
    encode(substr(jobdata, 89, 26), 'escape') AS tstamp,
    encode(substr(jobdata, 115, 8), 'escape') AS t1,
    encode(substr(jobdata, 123, 36), 'escape') AS t2,
    encode(substr(jobdata, 159, 2), 'escape') AS t3,length(substr(jobdata, 161)) as t4
FROM job_scheduler;






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

CREATE TABLE sysinfo (system_name CHAR(10) PRIMARY key,
                        ipaddress BIGINT NOT NULL,
                        systems_capacity INTEGER NOT NULL);

INSERT INTO sysinfo VALUES('M2', '123456', 2);






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







WITH cte_jobdata AS (
    SELECT 
        jobid, 
        jtype, 
        jsource, 
        jdestination, 
        jpriority,
        seqnum, 
        substring(jobdata, seqnum * systems_capacity, systems_capacity) AS subdata
    FROM job_scheduler
    JOIN sysinfo ON encode(substr(job_scheduler.jobdata, 79, 5), 'escape') = sysinfo.system_name
    CROSS JOIN generate_series(0, length(jobdata) / systems_capacity) AS seqnum
    WHERE jstate = 'S-2'
), cte_sent_msg AS (
    SELECT 
        create_message(jtype::text, 
                        lpad(seqnum::text, 8, '0')::bytea || jobid::text::bytea || length(subdata)::int::text::bytea || subdata, 
                        jsource::text, 
                        jdestination::text, 
                        jpriority::text) AS sent_msg,
        jobid, jtype, jsource, jdestination, jpriority, seqnum
    FROM cte_jobdata
)
INSERT INTO job_scheduler (jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority)
SELECT 
    sent_msg, 'S-3', '2', jsource, encode(substr(sent_msg, 33, 36), 'escape')::uuid, jobid, jdestination, jpriority 
FROM cte_sent_msg;































-- SELECT process_receivers_comms();

-- CREATE OR REPLACE FUNCTION process_receivers_comms () 
-- RETURNS void AS
-- $$
-- DECLARE
--     ltransaction_string text;
-- BEGIN

--     SELECT transaction_text into ltransaction_string FROM transactions WHERE transactionid = 1; 
--     EXECUTE ltransaction_string;

-- END;
-- $$
-- LANGUAGE 'plpgsql';

-- -- INSERT INTO transactions (transactionid, transaction_text)
-- -- VALUES (1, '
-- --             WITH deleted_rows AS 
-- --             ( 
-- --               DELETE FROM receivers_comms 
-- --               WHERE mtype = 1
-- --               RETURNING mdata
-- --             )
-- --             INSERT INTO receiving_conns (rfd, ripaddr, rcstatus)
-- --             SELECT get_byte(mdata, 1)::bit(8)::int + 
-- --                 (get_byte(mdata::bytea, 2)::bit(8)::int << 8) +
-- --                 (get_byte(mdata::bytea, 3)::bit(8)::int << 16) +
-- --                 (get_byte(mdata::bytea, 4)::bit(8)::int << 24), 
-- --                 get_byte(mdata::bytea, 5)::bit(8)::int +
-- --                 (get_byte(mdata::bytea, 6)::bit(8)::int << 8 ) +
-- --                 (get_byte(mdata::bytea, 7)::bit(8)::int << 16) +
-- --                 (get_byte(mdata::bytea, 8)::bit(8)::int << 24),
-- --                 1 FROM deleted_rows;
-- --           '
-- --         );


-- select md5(substr(data, 33, 79)) from temp1;

-- select create_message('01'::text, 'hello'::text, 's1'::text, 's2'::text, '05'::text);

-- select encode(gen_random_uuid::text:bytea, 'escape');
-- select encode(now()::text::bytea, 'escape');


-- -- in c query
-- INSERT INTO job_scheduler(jobdata, jstate, jtype, jsource_ip, jobid, jparent_jobid, jdestination_ip, jpriority) VALUES($2, 'N-1', 0, $1, GEN_RANDOM_UUID(), (select jobid from job_scheduler where jidx = 1), 0, 0);

-- select * from cron.job_run_details;
-- delete from cron.job_run_details;

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















insert into temp1(data) select create_message('N-1', 'helo', 'S-3', 'S-44', '3');

SELECT                           
    encode(substr(jobdata, 1, 32),'escape') AS hash,
    encode(substr(jobdata, 33, 36),'escape') AS uuid,
    encode(substr(jobdata, 69, 5), 'escape') AS message_type,
    encode(substr(jobdata, 75, 5), 'escape') AS message_source,
    encode(substr(jobdata, 79, 5), 'escape') AS message_destination,
    encode(substr(jobdata, 84, 5),'escape') AS message_priority,
    encode(substr(jobdata, 89, 26), 'escape') AS timestamp,
    encode(substr(jobdata, 115), 'escape') AS message
FROM job_scheduler;



