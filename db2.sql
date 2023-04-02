
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

CREATE TABLE job_scheduler (jidx SERIAL PRIMARY KEY, 
                        jobdata bytea NOT NULL,
                        jstate CHAR(5) NOT NULL DEFAULT 'N-1',
                        jtype SMALLINT NOT NULL DEFAULT 1,
                        jsource_ip bytea NOT NULL,
                        jobid UUID UNIQUE NOT NULL,
                        jparent_jobid UUID REFERENCES job_scheduler(jobid) ON UPDATE CASCADE ON DELETE CASCADE,
                        jdestination_ip BIGINT NOT NULL DEFAULT 0,
                        jpriority SMALLINT NOT NULL DEFAULT 10,
                        jcreation_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP);


INSERT INTO job_scheduler
(jobdata, jstate, jtype, jsource_ip, jobid, jparent_jobid, jdestination_ip, jpriority) 
VALUES('__ROOT__', 'N-0', 0, '0'::bytea, GEN_RANDOM_UUID(), NULL, 0, 0);

UPDATE job_scheduler 
SET jparent_jobid = jobid 
WHERE jidx = 1;

ALTER TABLE job_scheduler 
ALTER COLUMN jparent_jobid
SET NOT NULL;
































CREATE OR REPLACE FUNCTION process_receivers_comms () 
RETURNS void AS
$$
DECLARE
    ltransaction_string text;
BEGIN

    SELECT transaction_text into ltransaction_string FROM transactions WHERE transactionid = 1; 
    EXECUTE ltransaction_string;

END;
$$
LANGUAGE 'plpgsql';

-- INSERT INTO transactions (transactionid, transaction_text)
-- VALUES (1, '
--             WITH deleted_rows AS 
--             ( 
--               DELETE FROM receivers_comms 
--               WHERE mtype = 1
--               RETURNING mdata
--             )
--             INSERT INTO receiving_conns (rfd, ripaddr, rcstatus)
--             SELECT get_byte(mdata, 1)::bit(8)::int + 
--                 (get_byte(mdata::bytea, 2)::bit(8)::int << 8) +
--                 (get_byte(mdata::bytea, 3)::bit(8)::int << 16) +
--                 (get_byte(mdata::bytea, 4)::bit(8)::int << 24), 
--                 get_byte(mdata::bytea, 5)::bit(8)::int +
--                 (get_byte(mdata::bytea, 6)::bit(8)::int << 8 ) +
--                 (get_byte(mdata::bytea, 7)::bit(8)::int << 16) +
--                 (get_byte(mdata::bytea, 8)::bit(8)::int << 24),
--                 1 FROM deleted_rows;
--           '
--         );



SELECT process_receivers_comms();



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



select create_message('01'::text, 'hello'::text, 's1'::text, 's2'::text, '05'::text);

select encode(gen_random_uuid::text:bytea, 'escape');
select encode(now()::text::bytea, 'escape');


CREATE OR REPLACE FUNCTION create_message(
    message_type char(5),
    messaget text,
    message_source char(5),
    message_destination char(5),
    message_priority char(5)
) RETURNS bytea
AS $$
DECLARE   
    hnmessage bytea;
BEGIN
    hnmessage :=  gen_random_uuid()::text::bytea || message_type::bytea || message_source::bytea || message_destination::bytea || message_priority::bytea || to_char(now(), 'YYYY-MM-DD HH24:MI:SS.US')::bytea || messaget::bytea;
    RETURN md5(hnmessage)::bytea || hnmessage;
END;
$$ LANGUAGE plpgsql;




INSERT INTO job_scheduler(jobdata, jstate, jtype, jsource_ip, jobid, jparent_jobid, jdestination_ip, jpriority) 
VALUES(
    (select create_message('01'::text, 'hello'::text, 's1'::text, 's2'::text, '05'::text)),
     'N-1', 0, '0'::bytea, GEN_RANDOM_UUID(), (select jobid from job_scheduler where jidx = 1), 0, 5);

INSERT INTO job_scheduler(jobdata, jstate, jtype, jsource_ip, jobid, jparent_jobid, jdestination_ip, jpriority) 
VALUES(
    (select create_message('01'::text, 'hello'::text, 's1'::text, 's2'::text, '05'::text)),
     'N-1', 0, '0'::bytea, GEN_RANDOM_UUID(), (select jobid from job_scheduler where jidx = 1), 0, 5);

UPDATE job_scheduler set jobdata = substr(jobdata, 2) where jidx > 1 and jidx%2 != 0;




UPDATE job_scheduler set jstate = 'N-2' where jstate = 'N-1' and encode(substr(jobdata, 1, 32), 'escape') = md5(substr(jobdata, 33));
UPDATE job_scheduler set jstate = 'D' where jstate = 'N-1' and encode(substr(jobdata, 1, 32), 'escape') != md5(substr(jobdata, 33));


UPDATE job_scheduler 
SET jstate = 
    CASE 
        WHEN jstate = 'N-1' AND encode(substr(jobdata, 1, 32), 'escape') = md5(substr(jobdata, 33)) THEN 'N-2'
        WHEN jstate = 'N-1' AND encode(substr(jobdata, 1, 32), 'escape') != md5(substr(jobdata, 33)) THEN 'D'
        WHEN jstate = 'N-2' AND encode(substr(jobdatat, 73, 2), 'escape') = (SELECT jdestination_ip FROM job_scheduler WHERE jidx= 1) THEN 'N-3'
        WHEN jstate = 'N-2' AND encode(substr(jobdatat, 73, 2), 'escape') != (SELECT jdestination_ip FROM job_scheduler WHERE jidx= 1) THEN 'S-1'
        ELSE jstate
    END;


select md5(substr(data, 33, 79)) from temp1;

SELECT                           
    encode(substr(jobdata, 1, 32),'escape') AS hash,
    encode(substr(jobdata, 33, 36),'escape') AS uuid,
    encode(substr(jobdata, 69, 2), 'escape')::text AS message_type,
    encode(substr(jobdata, 71, 2), 'escape')::text AS message_source,
    encode(substr(jobdata, 73, 2), 'escape') AS message_destination,
    encode(substr(jobdata, 75, 2),'escape')::text AS message_priority,
    encode(substr(jobdata, 76, 32), 'escape') AS timestamp,
    encode(substr(jobdata, 109), 'escape') AS message
FROM job_scheduler;




























-- in c query
INSERT INTO job_scheduler(jobdata, jstate, jtype, jsource_ip, jobid, jparent_jobid, jdestination_ip, jpriority) VALUES($2, 'N-1', 0, $1, GEN_RANDOM_UUID(), (select jobid from job_scheduler where jidx = 1), 0, 0);

shrikant=# select * from cron.job_run_details;
shrikant=# delete from cron.job_run_details;

delete from cron.job where jobid = 1;


-- inside postgresql.conf

shared_preload_libraries='pg_cron' 
cron.database_name='shrikant'

-- inside pg_hba.conf
# "local" is for Unix domain socket connections only
local   all             all                                     peer
# IPv4 local connections:
host    all             all             127.0.0.1/32            trust
# IPv6 local connections:
host    all             all             ::1/128                 trust










