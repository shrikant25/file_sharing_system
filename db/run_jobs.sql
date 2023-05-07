

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

INSERT INTO job_scheduler (jobid,  jobdata, jstate, jtype, jsource, jparent_jobid, jdestination, jpriority) 
VALUES (gen_random_uuid(), 'hola', 'N-1', 0, lpad('M2', 5, ' '), 
    (SELECT jobid FROM job_scheduler WHERE jobid = jparent_jobid), lpad('m3', 5, ' '), 5);

INSERT INTO sysinfo_comms (sipaddr, port, capacity, sctype)
SELECT si.ipaddress, si.comssport,  (SELECT system_capacity FROM SELFINO), 1
FROM sysinfo si 
JOIN job_scheduler js 
ON js.jdestination = si.system_name
WHERE dataport = 0 AND
js.jstate = 'S-1'; 


UPDATE job_scheduler 
SET jstate = 'S-2' 
WHERE jobdata = ( SELECT file_name::bytea 
                FROM files JOIN sysinfo si ON
                jdestination = si.system_name AND
                LENGTH(lo_get(file_data)) > system_capacity)
AND jstate = 'S-1';

UPDATE job_scheduler 
SET jstate = 'S-3' 
WHERE jobdata = ( SELECT file_name::bytea 
                FROM files JOIN sysinfo si ON
                jdestination = si.system_name AND
                LENGTH(lo_get(file_data)) <= system_capacity)
AND jstate = 'S-1';



-- For all jobs where size of data greater than capacity, 
-- divide the job in n jobs such that each jobs which will have size same as the message size
-- except for last message it may have the same size of less

WITH par_job AS (
    
    SELECT fd.file_data, js.jobid, as parent_jobid length(lo_get(file_data)) datal, 
    js.jdestination, js.jsource, js.jpriority, si.system_capacity FROM 
    files fd JOIN
    job_scheduler js ON 
    fd.file_name::bytea = js.jobdata
    JOIN sysinfo si ON
    js.jdestination = si.system_name
    WHERE jstate = 'S-2'

),
chunk_info AS (
    SELECT idx, gen_random_uuid()::text::bytea AS uuid_data, 
    parent_jobid, jdestination, jsource, jpriority, system_capacity,  
    lo_get(file_data, (idx*system_capacity)::BIGINT, system_capacity::INTEGER) chunk_data 
    FROM par_job, generate_series(0, ceil((datal)::decimal/(system_capacity - 168))-1) idx
)
INSERT INTO job_scheduler (jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority)
        SELECT
            create_message( uuid_data, '2'::text, 
                            lpad(idx::text, 8, ' ')::bytea || parent_jobid::text::bytea || lpad(length(chunk_data)::text, 10, ' ')::bytea, 
                            chunk_data, btrim(jsource, ' '), btrim(jdestination, ' '),
                            jpriority::text),
            'S-3', '2', jsource, 
            encode(uuid_data, 'escape')::uuid, 
            parent_jobid,
            jdestination, 
            jpriority 
        FROM chunk_info;




-- for every message that is split into smaller chunks, create a info message
-- insert that message in scheduler
WITH cte_msginfo as(

    SELECT create_message(  gen_random_uuid()::text::bytea, '3'::text,
                            js.jobid::text::bytea || lpad(length((lo_get(file_data)))::text, 12, ' ')::bytea || 
                            lpad((ceil(length(lo_get(file_data))::decimal/ (system_capacity -168)))::text, 10, ' ')::bytea,
                            ''::bytea, btrim(jsource, ' '), btrim(jdestination, ' '), jpriority::text)as mdata, 
        js.jobid, js.jdestination, js.jsource, js.jpriority, si.system_capacity
        FROM files fd JOIN
        job_scheduler js ON 
        fd.file_name::bytea = js.jobdata
        JOIN sysinfo si ON
        js.jdestination = si.system_name
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



-- take all fd which belong to an open connection,
-- but none of the job in scheduler have destination same as the ip that belong to those fd
-- and create a message to close them
INSERT INTO senders_comms (mdata1, mdata2, mtype)
SELECT sc.sfd, sc.sipaddr, 2
FROM sending_conns sc
LEFT JOIN job_scheduler js 
ON sc.sipaddr::text = js.jdestination 
AND js.jstate != 'C'
WHERE js.jdestination IS NULL
AND sc.scstatus = 2;






-- SELECT                       length(jobdata),    
--     encode(substr(jobdata, 89, 26), 'escape') AS tm,
--     encode(substr(jobdata, 115, 36), 'escape') AS parentjobid, encode(substr(jobdata, 151, 12), 'escape') as size, encode(substr(jobdata, 163, 10), 'escape') as cnt  
-- FROM job_scheduler WHERE jstate = 'S-3';

-- SELECT                           
--     encode(substr(jobdata, 1, 32), 'escape') AS mhash,
--     encode(substr(jobdata, 33, 36),'escape') AS uuid,
--     encode(substr(jobdata, 69, 5), 'escape') AS message_type,
--     encode(substr(jobdata, 74, 5), 'escape') AS message_source,
--     encode(substr(jobdata, 79, 5), 'escape') AS message_destination,
--     encode(substr(jobdata, 84, 5), 'escape') AS message_priority, 
--     encode(substr(jobdata, 89, 26), 'escape') AS tm,
--     encode(substr(jobdata, 115, 8), 'escape') AS chunk_count,
--     encode(substr(jobdata, 123, 36), 'escape') AS jobid,
--     encode(substr(jobdata, 159, 10), 'escape') AS datalength,
--     encode(substr(jobdata, 169), 'escape') AS data 
-- FROM job_scheduler WHERE jstate = 'S-3';



-- insert into job_scheduler (jobid, jobdata, jstate, jtype, jsource, jparent_jobid, jdestination, jpriority)  
-- values (gen_random_uuid(), 'hola', 'S-1', 0, '   M2', '02f7e36a-d138-4458-978a-911b4bc8bb19', '   m3', 5);

-- insert into file_data (file_id, file_name, file_data) values(gen_random_uuid(), 'hola', lo_import('/home/shrikant/Desktop/prj/test/f9.mkv'));
-- cte_jobdata = splits the messages into smaller chunks based on capacity of receiver
-- cte_msg = creates a specific header for every chunk
-- insert the new message in job scheduler



-- WITH cte_jobdata AS (

--     SELECT 
--         jobid, 
--         jdestination, 
--         encode(substr(js.jobdata, 74, 5), 'escape') as jsource,
--         jpriority,
--         seqnum, 
--         substring(jobdata, (seqnum * (system_capacity-168) +1)::int, (system_capacity-168)) AS subdata
    
--     FROM job_scheduler js
--     JOIN sysinfo si
--     ON encode(substr(js.jobdata, 79, 5), 'escape') = si.system_name
--     CROSS JOIN generate_series(0, ceil(length(js.jobdata)::decimal/ (si.system_capacity -168))-1) AS seqnum
--     WHERE js.jstate = 'S-2' and si.ipaddress::text = js.jdestination
-- ), 
-- cte_msg AS (
--     SELECT  
--             create_message('2'::text, 
--                 lpad(seqnum::text, 8, ' ')::bytea || jobid::text::bytea || lpad(length(subdata)::text, 10, ' ')::bytea || subdata, 
--                 btrim(jsource, ' '), 
--                 btrim(jdestination, ' '),
--                 jpriority::text) AS msg,
--             jobid, 
--             jsource,
--             jdestination, 
--             jpriority, 
--             seqnum
--     FROM cte_jobdata
-- )
-- INSERT INTO job_scheduler (jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority)
-- SELECT msg, 'S-3', '2', jsource, encode(substr(msg, 33, 36), 'escape')::uuid, jobid, jdestination, jpriority 
-- FROM cte_msg;





-- jobs that have larger size then receivers capacity, update them to state S-2 
-- or else update them to state S-3

-- UPDATE job_scheduler AS js
-- SET jstate = (
--     SELECT 
--         CASE 
--             WHEN LENGTH(jobdata) > (SELECT MAX(system_capacity)
--                                     FROM sysinfo si 
--                                     JOIN systems sy ON si.system_name = sy.system_name
--                                     WHERE sy.system_name = js.jdestination)                        
--             THEN 'S-2'
--             ELSE 'S-3'
--         END
-- ) WHERE jstate = 'S-1';

-- -- replace the system name with its ip address 

-- UPDATE job_scheduler
-- SET jdestination = (
--         SELECT ipaddress::text 
--         FROM sysinfo sy
--         JOIN job_scheduler js
--         ON sy.system_name = js.jdestination
--         AND LENGTH(js.jobdata) >= system_capacity
--         ORDER BY system_capacity ASC
--         LIMIT 1
--     )
-- WHERE jstate = 'S-3';




-- UPDATE job_scheduler 
-- SET jdestination = (
--     SELECT
--         CASE
--             WHEN LENGTH(jobdata) > MAX(system_capacity)
--             THEN (
--                 SELECT ipaddress::text 
--                 FROM sysinfo sy 
--                 JOIN (
--                     SELECT system_name, MAX(system_capacity) AS max_capacity 
--                     FROM sysinfo 
--                     GROUP BY system_name
-- -- updates state of message from N-1 to N-2 if md5 hash matches, otherewise the message is marked as dead
-- UPDATE job_scheduler 
-- SET jstate = 
--     CASE 
--         WHEN jstate = 'N-1' AND 
--             encode(substr(jobdata, 1, 32), 'escape') = md5(substr(jobdata, 33)) 
--         THEN 'N-2'
--         WHEN jstate = 'N-1' AND 
--             encode(substr(jobdata, 1, 32), 'escape') != md5(substr(jobdata, 33)) 
--         THEN 'D'
--         ELSE jstate
--     END;


-- -- if message is for same system, then update the state to N-3
-- UPDATE job_scheduler 
-- SET jstate = 'N-3' 
-- WHERE jstate = 'N-2' 
-- AND 
-- encode(substr(jobdata, 79, 5), 'escape') = (SELECT jdestination FROM job_scheduler WHERE jobid = jparent_jobid);


-- -- if message is for some other system, then update the state to S-1 
-- UPDATE job_scheduler 
-- SET jstate = 'S-1', 
-- jdestination = encode(substr(jobdata, 79, 5), 'escape') 
-- WHERE jstate = 'N-2' 
-- AND 
-- encode(substr(jobdata, 79, 5), 'escape') != (SELECT jdestination FROM job_scheduler WHERE jobid = jparent_jobid);


-- -- identify the type, and update the state to N-4
-- UPDATE job_scheduler 
-- SET jstate = 'N-4', 
-- jtype = encode(substr(jobdata, 69, 5), 'escape')
-- WHERE jstate = 'N-3';


-- -- jobs that have larger size then receivers capacity, update them to state S-2 
-- -- or else update them to state S-3

-- UPDATE job_scheduler AS js
-- SET jstate = (
--     SELECT 
--         CASE 
--             WHEN LENGTH(jobdata) > (SELECT MAX(system_capacity)
--                                     FROM sysinfo si 
--                                     JOIN systems sy ON si.system_name = sy.system_name
--                                     WHERE sy.system_name = js.jdestination)                        
--             THEN 'S-2'
--             ELSE 'S-3'
--         END
-- ) WHERE jstate = 'S-1';

-- -- replace the system name with its ip address 

-- UPDATE job_scheduler 
-- SET jdestination = (
--     SELECT
--         CASE
--             WHEN LENGTH(jobdata) > MAX(system_capacity)
--             THEN (
--                 SELECT ipaddress::text 
--                 FROM sysinfo sy 
--                 JOIN (
--                     SELECT system_name, MAX(system_capacity) AS max_capacity 
--                     FROM sysinfo 
--                     GROUP BY system_name
--                 ) sy2 ON sy.system_name = sy2.system_name AND sy.system_capacity = sy2.max_capacity
--                 WHERE sy.system_name = job_scheduler.jdestination
--                 LIMIT 1
--             )
--             ELSE (
--                 SELECT ipaddress::text 
--                 FROM (
--                     SELECT ipaddress, system_capacity 
--                     FROM sysinfo 
--                     WHERE system_name = job_scheduler.jdestination AND LENGTH(jobdata) <= system_capacity 
--                     ORDER BY system_capacity ASC
--                 ) ip 
--                 GROUP BY system_capacity, ipaddress 
--                 ORDER BY system_capacity DESC 
--                 LIMIT 1
--             )
--         END
--     FROM sysinfo 
--     WHERE sysinfo.system_name = job_scheduler.jdestination
-- ) 
-- WHERE jstate IN ('S-3', 'S-2');






-- -- cte_jobdata = splits the messages into smaller chunks based on capacity of receiver
-- -- cte_msg = creates a specific header for every chunk
-- -- insert the new message in job scheduler
-- WITH cte_jobdata AS (

--     SELECT 
--         jobid, 
--         jdestination, 
--         encode(substr(js.jobdata, 74, 5), 'escape') as jsource,
--         jpriority,
--         seqnum, 
--         substring(jobdata, (seqnum * (system_capacity-114) +1)::int, (system_capacity-114)) AS subdata
    
--     FROM job_scheduler js
--     JOIN sysinfo si
--     ON encode(substr(js.jobdata, 79, 5), 'escape') = si.system_name
--     CROSS JOIN generate_series(0, ceil(length(js.jobdata)::decimal/ (si.system_capacity -114))-1) AS seqnum
--     WHERE js.jstate = 'S-2' and si.ipaddress::text = js.jdestination
-- ), 
-- cte_msg AS (
--     SELECT  
--             create_message('2'::text, 
--                 lpad(seqnum::text, 8, ' ')::bytea || jobid::text::bytea || lpad(length(subdata)::text, 10, ' ')::bytea || subdata, 
--                 btrim(jsource, ' '), 
--                 btrim(jdestination, ' '),
--                 jpriority::text) AS msg,
--             jobid, 
--             jsource,
--             jdestination, 
--             jpriority, 
--             seqnum
--     FROM cte_jobdata
-- )
-- INSERT INTO job_scheduler (jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority)
-- SELECT msg, 'S-3', '2', jsource, encode(substr(msg, 33, 36), 'escape')::uuid, jobid, jdestination, jpriority 
-- FROM cte_msg;



-- -- for every message that is split into smaller chunks, create a info message
-- -- insert that message in scheduler
-- WITH cte_msginfo as(

--     SELECT
--         create_message(
--                 '3'::text,
--                 jobid::text::bytea || lpad(length(jobdata)::text, 10, ' ')::bytea || 
--                     lpad((ceil(length(jobdata)::decimal/ (system_capacity -114)))::text, 10, ' ')::bytea,
--                 btrim(encode(substr(job_scheduler.jobdata, 74, 5), 'escape'), ' '), 
--                 btrim(jdestination, ' '),
--                 jpriority::text
--         )as mdata, 
--         jobid, 
--         jdestination, 
--         encode(substr(job_scheduler.jobdata, 74, 5), 'escape') as jsource,
--         jpriority
--         FROM job_scheduler
--     JOIN sysinfo 
--     ON encode(substr(job_scheduler.jobdata, 79, 5), 'escape') = sysinfo.system_name
--     WHERE jstate = 'S-2'
-- )
-- INSERT INTO job_scheduler (jobdata, jstate, jtype, jsource, jobid, jparent_jobid, jdestination, jpriority)
-- SELECT mdata, 'S-3', '3', jsource, encode(substr(mdata, 33, 36), 'escape')::uuid, jobid, jdestination, jpriority 
-- FROM cte_msginfo; 


-- -- since jobs is split into multiple jobs, send to waiting state
-- UPDATE job_scheduler SET jstate = 'S-2W' WHERE jstate = 'S-2';

-- -- create a record for every ipaddress where messages are intended for
-- -- and it is not present in sending_connections
-- INSERT into sending_conns (sfd, sipaddr, scstatus) 
-- SELECT DISTINCT -1, jdestination::BIGINT, 1  
-- FROM job_scheduler js 
-- WHERE js.jstate = 'S-3' 
-- AND NOT EXISTS (
--   SELECT 1 
--   FROM sending_conns sc
--   WHERE sc.sipaddr::text = js.jdestination
-- );   


-- -- take all record from sending_conns where state = 1(means closed) and
-- -- create a message for sender to open a connection for that particular ip
-- INSERT INTO senders_comms (mdata1, mdata2, mtype)
-- SELECT si.ipaddress, si.port, 1 
-- FROM sysinfo si
-- JOIN sending_conns sc 
-- ON si.ipaddress = sc.sipaddr
-- WHERE sc.scstatus = 1;


-- SELECT jobdata 
-- FROM job_scheduler js 
-- WHERE js.jstate = 'S-3'
-- AND 
