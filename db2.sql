
-- launch every second
SELECT pgcron.schedule('* * * * * *', 'call_jobs;');
-- call jobs function yet to be written


DROP TABLE sysinfo, receivers_comms, receiving_conns, senders_comms,
            sending_conns, logs,  query, job_scheduler; 


DROP TABLE receiving_conns, job_scheduler;

DROP TABLE receivers_comms, receiving_conns, transactions;

CREATE TABLE receivers_comms (rcomid SERIAL PRIMARY KEY, 
                              mdata bytea NOT NULL, 
                              mtype INTEGER NOT NULL DEFAULT 1);

CREATE TABLE receiving_conns (rconn SERIAL PRIMARY KEY, 
                              rfd INTEGER NOT NULL, 
                              ripaddr BIGINT NOT NULL, 
                              rcstatus INTEGER NOT NULL,
                              rctime TIMESTAMP DEFAULT CURRENT_TIMESTAMP);


CREATE TABLE sysinfo (system_name CHAR(10) PRIMARY key,
                        ipaddress BIGINT NOT NULL,
                        systems_capacity INTEGER NOT NULL);

CREATE TABLE transactions (transactionid INTEGER PRIMARY KEY,
                    transaction_text TEXT);


CREATE TABLE job_scheduler (jidx SERIAL PRIMARY KEY, 
                        jobdata TEXT NOT NULL,
                        jstate CHAR(5) NOT NULL DEFAULT 'N-1',
                        jtype SMALLINT NOT NULL DEFAULT 1,
                        jsource_ip BIGINT NOT NULL DEFAULT 0,
                        jobid UUID UNIQUE NOT NULL,
                        jparent_jobid UUID REFERENCES job_scheduler(jobid) ON UPDATE CASCADE ON DELETE CASCADE,
                        jdestination_ip BIGINT NOT NULL DEFAULT 0,
                        jpriority SMALLINT NOT NULL DEFAULT 10,
                        jcreation_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP);


INSERT INTO job_scheduler
(jobdata, jstate, jtype, jsource_ip, jobid, jparent_jobid, jdestination_ip, jpriority) 
VALUES('__ROOT__', 'N-0', 0, 0, GEN_RANDOM_UUID(), NULL, 0, 0);

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

INSERT INTO transactions (transactionid, transaction_text)
VALUES (1, '
            WITH deleted_rows AS 
            ( 
              DELETE FROM receivers_comms 
              WHERE mtype = 1
              RETURNING mdata
            )
            INSERT INTO receiving_conns (rfd, ripaddr, rcstatus)
            SELECT get_byte(mdata, 1)::bit(8)::int + 
                (get_byte(mdata::bytea, 2)::bit(8)::int << 8) +
                (get_byte(mdata::bytea, 3)::bit(8)::int << 16) +
                (get_byte(mdata::bytea, 4)::bit(8)::int << 24), 
                get_byte(mdata::bytea, 5)::bit(8)::int +
                (get_byte(mdata::bytea, 6)::bit(8)::int << 8 ) +
                (get_byte(mdata::bytea, 7)::bit(8)::int << 16) +
                (get_byte(mdata::bytea, 8)::bit(8)::int << 24),
                1 FROM deleted_rows;
          '
        );




SELECT process_receivers_comms();





CREATE TABLE sysinfo (system_name CHAR(10) PRIMARY key,
                        ipaddress BIGINT NOT NULL,
                        systems_capacity INTEGER NOT NULL);


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

CREATE TABLE transactions (transactionid INTEGER PRIMARY KEY,
                    transaction_text TEXT);


INSERT INTO transactions (transactionid, transaction_text)
VALUES (1, 'BEGIN;
         INSERT INTO recieving_conns (rfd, ripaddr, rcstatus)
         SELECT SUBSTRING(mdata, 1, 4)::int, SUBSTRING(mdata, 5, 4)::int, 1 FROM receiving_comms WHERE mtype = 1;
         DELETE FROM receiving_comms WHERE mtype = 1;
         COMMIT;');

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

CREATE OR REPLACE FUNCTION call_jobs()
RETURNS void AS
'
BEGIN
    SELECT build_msg();
END;
'
LANGUAGE'plpgsql';















CREATE OR REPLACE FUNCTION create_message(
    message_type text,
    messaget text,
    message_source text,
    message_destination text,
    message_priority text
) RETURNS bytea
AS $$
DECLARE   
    hnmessage bytea;
BEGIN
    hnmessage :=  gen_random_uuid()::text::bytea || message_type::bytea || message_source::bytea || message_destination::bytea || message_priority::bytea ||  (now())::text::bytea || messaget::bytea;
    RETURN md5(hnmessage)::bytea || hnmessage;
END;
$$;
LANGUAGE plpgsql

select create_message('01'::text, 'hellogal'::text, 's1'::text, 's2'::text, '05'::text);

select encode(gen_random_uuid::text:bytea, 'escape');
select encode(now()::text::bytea, 'escape');











CREATE TRIGGER tr_extract_senders_comms 
AFTER INSERT ON senders_comms 
FOR EACH ROW EXCUTE extract_senders_comms();

CREATE FUNCTION extract_senders_comms() 
RETURNS void AS
'
DECLARE

    lmsgtype integer;
    lquery text;

BEGIN

    SELECT NEW.mtype into lmsgtype;
    
    SELECT query INTO lquery 
    FROM queries 
    WHERE queryid = lmsgtype;
    
    EXECUTE query;
    
END;
'
LANGUAGE 'plpgsql';









CREATE OR REPLACE FUNCTION build_msg() 
RETURNS void AS
'
DECLARE
    lfd int;
    ldata text;
    ldata_size int;
    ldata_read int;
    lmdata_size int;
BEGIN
    
    SELECT rfd, rdata, rdata_size FROM raw_data into lfd, ldata, ldata_size;
    ldata_read := 0;

    WHILE ldata_size - ldata_read >= 40 LOOP
      
        SELECT into lmdata_size SUBSTRING(ldata, 1+ldata_read, 4)::int;
        RAISE NOTICE ''v : %'', lmdata_size;
        IF ldata_size - ldata_read >= lmdata_size THEN
            
            INSERT INTO job_scheduler (jobdata, jstate, jtype, jsource_ip, 
                    jobid, jparent_jobid, jdestination_ip, jpriority) 
            VALUES (
                SUBSTRING(ldata, ldata_read+1, lmdata_size),
                ''N-1'',
                1,
                (SELECT ripaddr FROM receiving_conns WHERE rfd = lfd),
                (SELECT GEN_RANDOM_UUID()),
                (SELECT jobid FROM job_scheduler WHERE jidx = 1),
                0,
                5
            );

            ldata_read := ldata_read + lmdata_size;
        END IF;
    END LOOP;

    IF ldata_read > 0 THEN
        
        UPDATE raw_data 
        SET rdata = SUBSTRING(rdata, ldata_read+1, ldata_size - ldata_read),
        rdata_size = ldata_size - ldata_read, 
        rdpriority = 10
        WHERE rfd = lfd;

    END IF;

END;
'
LANGUAGE 'plpgsql';


SELECT build_msg();




CREATE TRIGGER tr_update_chunk_count 
AFTER INSERT ON msg_chunk
FOR EACH ROW EXECUTE update_chunk_count();

CREATE OR REPLACE FUNCTION update_chunk_count()
RETURNS void AS
'
BEGIN
    INSERT INTO msg_info (original_msgid, parts_received)
    VALUES (new.original_msgid, 1) 
    ON CONFLICT (original_msgid)
    DO UPDATE
    SET parts_received = parts_received + 1;  
END;
'
LANGUAGE 'plpgsql'; 


CREATE FUNCTION merge_msg()
RETURN void AS
'
DECLARE 
    msg text;
    invalid_chunks int[];
BEGIN

    IF new.total_parts > 0 and new.parts_recieved = new.total_parts THEN

        SELECT array_agg(C.chunk_number)
        INTO invalid_chunks 
        FROM (
            SELECT row_number() over() 
            as row_numb, md5_elem
            FROM unnset(NEW.md5_list) 
            AS md5_elem
        ) AS M
        JOIN msg_chunk AS C
        ON C.chunk_number = M.row_num
        WHERE C.md5_hash <> M.md5_elem;

        IF array_length(invalid_chunkd) > 0 THEN
        
            SELECT handle_invalid_chunks(invalid_chunks);
        
        ELSE
        
            INSERT INTO msg (msgid, source, size, mdata) 
            SELECT NEW.original_msgid, 
                NEW.source,
                NEW.total_size,
                string_agg(m.mdata, ''''), 
            FROM msg_chunk as m 
            WHERE NEW.original_msgid = m.original_msgid
            group by m.original_msgid
            order by m.chunk_number;

        ENDIF;
    ENDIF;
END;
'
LANGUAGE 'plpgsql';

CREATE FUNCTION extract_msg_info() 
RETURNS void AS
'
DECLARE
    l_nmdata text;
    l_nmid int;
    l_query text;
    
BEGIN
    
    SELECT nmdata, nmgid 
    INTO l_nmdata, l_nmid 
    FROM new_msg 
    WHERE status = 2 
    LIMIT 1;

    SELECT query 
    FROM query_table 
    WHERE queryid = SUBSTRING(l_nmdata, 5, 4)::int 
    INTO l_query;

    EXECUTE query;

    DELETE FROM new_msg 
    WHERE nmgid = l_nmid;

END;
'
LANGUAGE 'plpgsql';


CREATE TRIGGER tr_merge_msg
AFTER INSERT, UPDATE ON msg_info
FOR EACH ROW EXECUTE merged_msg();




CREATE TRIGGER tr_extract_msg_info 
AFTER INSERT ON new_msg 
FOR EACH ROW EXCUTE extract_msg_info();

INSERT INTO query VALUES (1, 'INSERT INTO msg_chunk VALUES(
                            SUBSTRING( l_nmdata, 9, 16),
                            SUBSTRING( l_nmdata, 25, 8)::bigint,
                            SUBSTRING( l_nmdata, 33, 8)::bigint,
                            SUBSTRING( l_nmdata, 41, 4)::int,
                            SUBSTRING( l_nmdata, 5, 4)::int,
                            SUBSTRING( l_nmdata, 45, 16),
                            SUBSTRING( l_nmdata, 61, 4)::int,
                            SUBSTRING( l_nmdata, 65, 4)::int,
                            SUBSTRING( l_nmdata, 69, 4)::int,
                            SUBSTRING( l_nmdata, 73, SUBSTRING(l_mdata, 1, 4)::int - 72)
                            )'
                        );


INSERT INTO query VALUES (2, 'INSERT INTO msg_info VALUES(
                            SUBSTRING( l_nmdata, 9, 16),
                            SUBSTRING( l_nmdata, 25, 8)::bigint,
                            SUBSTRING( l_nmdata, 33, 8)::bigint,
                            SUBSTRING( l_nmdata, 41, 4)::int,
                            SUBSTRING( l_nmdata, 5, 4)::int,
                            SUBSTRING( l_nmdata, 45, 16),
                            SUBSTRING( l_nmdata, 61, 4)::int,
                            SUBSTRING( l_nmdata, 65, 4)::int,
                            SUBSTRING( l_nmdata, 69, 4)::int
                            )'
                        );

INSERT INTO query VALUES (3, 'INSERT INTO get_info VALUES(
                            SUBSTRING( l_nmdata, 9, 16),
                            SUBSTRING( l_nmdata, 25, 4)::int,
                            SUBSTRING( l_nmdata, 29, 4)::int,
                            SUBSTRING( l_nmdata, 32, SUBSTRING(l_mdata, 1, 4)::int - 72)
                            )'
                        );

INSERT INTO query VALUES (4, 'INSERT INTO get_info VALUES(
                            SUBSTRING( l_nmdata, 9, 16),
                            SUBSTRING( l_nmdata, 33, 8)::int,
                            SUBSTRING( l_nmdata, 1, 4)::int,
                            l_nmdata
                            )'
                        );












--CREATE TABLE raw_data (rfd INTEGER PRIMARY KEY, 
--                       rdata TEXT NOT NULL, 
--                       rdata_size INTEGER NOT NULL,
--                       rtime TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
--                       rdpriority INTEGER NOT NULL);

-- INSERT INTO raw_data(rfd, rdata, rdata_size, rdpriority) VALUES(1, '0042aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaahello0043aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaahelloZLMNOP', 90, 5);






CREATE FUNCTION f(x integer) RETURNS set of t1 AS 
'
BEGIN
select c1 from t1 limit 4 offset $1;
return;
END;
' 
LANGUAGE 'plpgsql';
