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
encode(substr(jobdata, 79, 5), 'escape') = (SELECT jdestination FROM job_scheduler WHERE jidx= 1);


-- if message is for some other system, then update the state to S-1 
UPDATE job_scheduler 
SET jstate = 'S-1', 
jdestination = encode(substr(jobdata, 79, 5), 'escape') 
WHERE jstate = 'N-2' 
AND 
encode(substr(jobdata, 79, 5), 'escape') != (SELECT jdestination FROM job_scheduler WHERE jidx= 1);


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
            WHEN LENGTH(jobdata) > systems_capacity 
            THEN 'S-2'
            ELSE 'S-3'
        END
    FROM sysinfo
    WHERE system_name = js.jdestination
)
WHERE jstate = 'S-1';


-- replace the system name with its ip address 
UPDATE job_scheduler 
SET jdestination = (SELECT ipaddress::text 
                    FROM sysinfo 
                    WHERE system_name = jdestination) 
WHERE jstate IN('S-3', 'S-2');


-- cte_jobdata = splits the messages into smaller chunks based on capacity of receiver
-- cte_msg = creates a specific header for every chunk
-- insert the new message in job scheduler
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



-- for every message that is split into smaller chunks, create a info message
-- insert that message in scheduler
WITH cte_msginfo as(

    SELECT
        create_message(
                '3'::text,
                jobid::text::bytea || lpad(length(jobdata)::text, 10, ' ')::bytea || 
                    lpad((ceil(length(jobdata)::decimal/ systems_capacity))::text, 10, ' ')::bytea,
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














