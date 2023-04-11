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


UPDATE job_scheduler 
SET jdestination = (SELECT ipaddress 
                    FROM sysinfo 
                    WHERE system_name = jdestination) 
WHERE jstate IN('S-3', 'S-2');



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



UPDATE sending_conns 
SET sfd = ($1), scstatus = ($2) 
WHERE scipddr = ($3);


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














