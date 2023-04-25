
DROP TABLE IF EXISTS logs, receivers_comms, receiving_conns, job_scheduler, sysinfo, systems, senders_comms, sending_conns;
DROP FUNCTION IF EXISTS send_noti(), create_message();
DROP TRIGGER IF EXISTS msg_for_sender1 ON job_scheduler;
DROP TRIGGER IF EXISTS msg_for_sender2 ON senders_comms;
UNLISTEN send_noti;


CREATE TABLE job_scheduler (jobid UUID PRIMARY KEY, 
                            jobdata bytea NOT NULL,
                            jstate CHAR(5) NOT NULL DEFAULT 'N-1',
                            jtype TEXT NOT NULL DEFAULT '1',
                            jsource TEXT NOT NULL,
                            jparent_jobid UUID 
                                REFERENCES job_scheduler(jobid) 
                                ON UPDATE CASCADE 
                                ON DELETE CASCADE,
                            jdestination TEXT NOT NULL DEFAULT 0,
                            jpriority  TEXT NOT NULL DEFAULT '10',
                            jcreation_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP);


CREATE TABLE receivers_comms (rcomid SERIAL PRIMARY KEY, 
                              mdata bytea NOT NULL, 
                              mtype INTEGER NOT NULL);

CREATE TABLE receiving_conns (rfd INTEGER PRIMARY KEY, 
                              ripaddr BIGINT NOT NULL, 
                              rcstatus SMALLINT NOT NULL,
                              rctime TIMESTAMP DEFAULT CURRENT_TIMESTAMP);

CREATE TABLE sending_conns (sfd INTEGER PRIMARY KEY,
                            sipaddr BIGINT NOT NULL, 
                            scstatus SMALLINT NOT NULL,
                            sctime TIMESTAMP DEFAULT CURRENT_TIMESTAMP);

CREATE TABLE senders_comms (scommid SERIAL PRIMARY KEY, 
                            mdata1 BIGINT NOT NULL,
                            mdata2 INTEGER NOT NULL, 
                            mtype SMALLINT NOT NULL);

CREATE TABLE logs (logid SERIAL PRIMARY KEY,
                   log TEXT NOT NULL,
                   lgtime TIMESTAMP DEFAULT CURRENT_TIMESTAMP);

CREATE TABLE systems(system_name CHAR(10) PRIMARY KEY);

CREATE TABLE sysinfo (system_name CHAR(10),
                      ipaddress BIGINT UNIQUE,
                      port INTEGER NOT NULL,
                      system_capacity INTEGER,
                      CONSTRAINT pk_sysinfo PRIMARY KEY(system_name, ipaddress),
                      CONSTRAINT fk_sys_capacity FOREIGN KEY (system_name)
                        REFERENCES  systems(system_name) 

                        ON DELETE CASCADE 
                        ON UPDATE CASCADE);

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


INSERT INTO job_scheduler
    (jobdata, jstate, jtype, jsource, 
        jobid, jparent_jobid, jdestination, jpriority) 
VALUES('__ROOT__', 'N-0', '0', '   M3', 
    GEN_RANDOM_UUID(), NULL, '   M3', 0);

UPDATE job_scheduler 
SET jparent_jobid = jobid 
WHERE jobdata = '__ROOT__';

ALTER TABLE job_scheduler 
ALTER COLUMN jparent_jobid
SET NOT NULL;

CREATE OR REPLACE FUNCTION send_noti()
RETURNS TRIGGER AS 
$$
BEGIN
    RAISE NOTICE, "hola";
    PERFORM pg_notify('senders_channel', 'get_data');
    RETURN NEW;
END;
$$
LANGUAGE plpgsql;


CREATE TRIGGER msg_for_sender1
AFTER UPDATE ON job_scheduler
FOR EACH ROW 
WHEN (NEW.jstate = 'S-3')
EXECUTE FUNCTION send_noti();


CREATE TRIGGER msg_for_sender2
AFTER INSERT on senders_comms
FOR EACH ROW 
WHEN (NEW.mtype = 1)
EXECUTE FUNCTION send_noti();


INSERT INTO job_scheduler(jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority) 
VALUES(
    (select create_message('01'::text, 'helloqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq'::bytea, 'M1'::text, 'M2'::text, '05'::text)),
     'N-1', '0', '0', GEN_RANDOM_UUID(), 
     (select jobid 
     from job_scheduler 
     where jparent_jobid = jobid), '0', '5');

INSERT into systems VALUES('   M2');
INSERT INTO sysinfo VALUES('   M2', 2130706433, 6000, 128*1024);



-- updates state of message from N-1 to N-2 if md5 hash matches, otherewise the message is marked as dead
UPDATE job_scheduler 
SET jstate = 
    CASE 
        WHEN jstate = 'N-1' AND 
            encode(substr(jobdata, 1, 32), 'escape') = md5(substr(jobdata, 33)) 
        THEN 'N-2'
        WHEN jstate = 'N-1' AND 
            encode(substr(jobdata, 1, 32), 'escape') != md5(substr(jobdata, 33)) 
        THEN 'D'
        ELSE jstate
    END;


-- if message is for same system, then update the state to N-3
UPDATE job_scheduler 
SET jstate = 'N-3' 
WHERE jstate = 'N-2' 
AND 
encode(substr(jobdata, 79, 5), 'escape') = (SELECT jdestination FROM job_scheduler WHERE jobid = jparent_jobid);


-- if message is for some other system, then update the state to S-1 
UPDATE job_scheduler 
SET jstate = 'S-1', 
jdestination = encode(substr(jobdata, 79, 5), 'escape') 
WHERE jstate = 'N-2' 
AND 
encode(substr(jobdata, 79, 5), 'escape') != (SELECT jdestination FROM job_scheduler WHERE jobid = jparent_jobid);


-- identify the type, and update the state to N-4
UPDATE job_scheduler 
SET jstate = 'N-4', 
jtype = encode(substr(jobdata, 69, 5), 'escape')
WHERE jstate = 'N-3';


-- jobs that have larger size then receivers capacity, update them to state S-2 
-- or else update them to state S-3

UPDATE job_scheduler AS js
SET jstate = (
    SELECT 
        CASE 
            WHEN LENGTH(jobdata) > (SELECT MAX(system_capacity)
                                    FROM sysinfo si 
                                    JOIN systems sy ON si.system_name = sy.system_name
                                    WHERE sy.system_name = js.jdestination)                        
            THEN 'S-2'
            ELSE 'S-3'
        END
) WHERE jstate = 'S-1';

-- replace the system name with its ip address 

UPDATE job_scheduler 
SET jdestination = (
    SELECT
        CASE
            WHEN LENGTH(jobdata) > MAX(system_capacity)
            THEN (
                SELECT ipaddress::text 
                FROM sysinfo sy 
                JOIN (
                    SELECT system_name, MAX(system_capacity) AS max_capacity 
                    FROM sysinfo 
                    GROUP BY system_name
                ) sy2 ON sy.system_name = sy2.system_name AND sy.system_capacity = sy2.max_capacity
                WHERE sy.system_name = job_scheduler.jdestination
                LIMIT 1
            )
            ELSE (
                SELECT ipaddress::text 
                FROM (
                    SELECT ipaddress, system_capacity 
                    FROM sysinfo 
                    WHERE system_name = job_scheduler.jdestination AND LENGTH(jobdata) <= system_capacity 
                    ORDER BY system_capacity ASC
                ) ip 
                GROUP BY system_capacity, ipaddress 
                ORDER BY system_capacity DESC 
                LIMIT 1
            )
        END
    FROM sysinfo 
    WHERE sysinfo.system_name = job_scheduler.jdestination
) 
WHERE jstate IN ('S-3', 'S-2');






-- cte_jobdata = splits the messages into smaller chunks based on capacity of receiver
-- cte_msg = creates a specific header for every chunk
-- insert the new message in job scheduler
WITH cte_jobdata AS (

    SELECT 
        jobid, 
        jdestination, 
        encode(substr(js.jobdata, 74, 5), 'escape') as jsource,
        jpriority,
        seqnum, 
        substring(jobdata, (seqnum * (system_capacity-114) +1)::int, (system_capacity-114)) AS subdata
    
    FROM job_scheduler js
    JOIN sysinfo si
    ON encode(substr(js.jobdata, 79, 5), 'escape') = si.system_name
    CROSS JOIN generate_series(0, ceil(length(js.jobdata)::decimal/ (si.system_capacity -114))-1) AS seqnum
    WHERE js.jstate = 'S-2' and si.ipaddress::text = js.jdestination
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



-- for every message that is split into smaller chunks, create a info message
-- insert that message in scheduler
WITH cte_msginfo as(

    SELECT
        create_message(
                '3'::text,
                jobid::text::bytea || lpad(length(jobdata)::text, 10, ' ')::bytea || 
                    lpad((ceil(length(jobdata)::decimal/ (system_capacity -114)))::text, 10, ' ')::bytea,
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


-- since jobs is split into multiple jobs, send to waiting state
UPDATE job_scheduler SET jstate = 'S-2W' WHERE jstate = 'S-2';

-- create a record for every ipaddress where messages are intended for
-- and it is not present in sending_connections
INSERT into sending_conns (sfd, sipaddr, scstatus) 
SELECT DISTINCT -1, jdestination::BIGINT, 1  
FROM job_scheduler js 
WHERE js.jstate = 'S-3' 
AND NOT EXISTS (
  SELECT 1 
  FROM sending_conns sc
  WHERE sc.sipaddr::text = js.jdestination
);   


-- take all record from sending_conns where state = 1(means closed) and
-- create a message for sender to open a connection for that particular ip
INSERT INTO senders_comms (mdata1, mdata2, mtype)
SELECT si.ipaddress, si.port, 1 
FROM sysinfo si
JOIN sending_conns sc 
ON si.ipaddress = sc.sipaddr
WHERE sc.scstatus = 1;