DROP TABLE IF EXISTS logs, receivers_comms, receiving_conns, job_scheduler, sysinfo, 
                        systems, senders_comms, sending_conns, file_data, selfinfo, sysinfo_comms;
DROP FUNCTION IF EXISTS send_noti(), create_message();
DROP TRIGGER IF EXISTS msg_for_sender1 ON job_scheduler;
DROP TRIGGER IF EXISTS msg_for_sender2 ON senders_comms;
UNLISTEN noti_1sys;


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

CREATE TABLE systems (system_name CHAR(10) PRIMARY KEY);

-- have proper naming convention for variabels
-- when checking on two different system only sysname and ip is required
-- comssport can be hardcoded value a hardcoded value and thus can be removed from sysinfo table
CREATE TABLE sysinfo (system_name CHAR(10),
                      ipaddress BIGINT UNIQUE,
                      dataport INTEGER,
                      comssport INTEGER NOT NULL,
                      system_capacity INTEGER,
                      CONSTRAINT pk_sysinfo PRIMARY KEY(system_name, ipaddress),
                      CONSTRAINT fk_sys_capacity FOREIGN KEY (system_name)
                        REFERENCES  systems(system_name) 
                        ON DELETE CASCADE 
                        ON UPDATE CASCADE);

CREATE TABLE selfinfo (system_name CHAR(10) NOT NULL,
                      ipaddress BIGINT UNIQUE NOT NULL,
                      dataport INTEGER NOT NULL,
                      system_capacity INTEGER NOT NULL);

CREATE TABLE sysinfo_comms (sicid SERIAL PRIMARY KEY,
                            sipaddr BIGINT NOT NULL,
                            port INTEGER NOT NULL,
                            capacity INTEGER NOT NULL,
                            sctype SMALLINT NOT NULL);

CREATE TABLE file_data (file_id UUID PRIMARY KEY, 
                        file_name TEXT UNIQUE NOT NULL,  
                        file_data oid NOT NULL, 
                        creation_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP); 

CREATE OR REPLACE FUNCTION create_message(
    uuid_data bytea,
    message_type text,
    subheader bytea,
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
    
    hnmessage := uuid_data || fixed_type::bytea || 
                fixed_source::bytea || fixed_destination::bytea || 
                fixed_priority::bytea ||  to_char(now(), 'YYYY-MM-DD HH24:MI:SS.US')::bytea || subheader;
    
    return md5(hnmessage || messaget)::bytea || hnmessage;
END;
$$
LANGUAGE 'plpgsql';

INSERT INTO selfinfo VALUES('M3', 2130706433, 7000, 128*1024);

INSERT INTO job_scheduler
    (jobdata, jstate, jtype, jsource, 
    jobid, jparent_jobid, jdestination, jpriority) 
    VALUES('__ROOT__', 'N-0', '0', lpad('M3', 5, ' '), 
    GEN_RANDOM_UUID(), NULL, lpad('M3', 5, ' '), 0);


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
    PERFORM pg_notify('noit_1sys', 'get_data');
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






