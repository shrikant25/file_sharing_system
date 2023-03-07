
-- launch every second
SELECT pgcron.schedule('* * * * * *', 'copy_message_ids;');


CREATE TABLE receivers_comms (rcid SERIAL PRIMARY KEY, 
                              mdata text NOT NULL, 
                              destination int NOT NULL DEFAULT 1);

CREATE TRIGGER tr_extract_receivers_comms AFTER INSERT ON 
receivers_comms FOR EACH ROW EXCUTE extract_receivers_comms();

CREATE OR REPLACE FUNCTION extract_receivers_comms () RETURNS void AS
'
DECLARE
    l_msgtype int;
BEGIN

    IF NEW.destination = 1 THEN        
        
        SELECT SUBSTRING(new.mdata, 1, 4)::int 
        INTO l_msgtype;
        
        IF msgtype = 1 THEN
            
            INSERT INTO connections_receiving(fd, ipaddr, rcstatus)
            VALUES (SUBSTRING(NEW.mdata, 5, 4)::int, 
            SUBSTRING(NEW.mdata, 9, 8)::bigint, 
            STATUS = 1);
        
        ELIF msgtype = 2 THEN
        
            UPDATE connections_receiving 
            SET rcstatus = 2 
            WHERE fd = SUBSTRING(NEW.mdata, 5, 4)::int;
        
        ENDIF;

        DELETE FROM receivers_comms 
        WHERE rcid = new.rcid;   
    
    ENDIF;
END;
'
LANGUAGE 'plpgsql';


CREATE TABLE connections_receiving (orid SERIAL PRIMARY KEY, 
                                    fd int NOT NULL, 
                                    ipaddr bigint NOT NULL, 
                                    rcstatus int NOT NULL);


--------


CREATE TABLE senders_comms (scid SERIAL PRIMARY KEY, 
                            mdata text NOT NULL, 
                            destination int NOT NULL DEFAULT 1);

CREATE TRIGGER tr_extract_senders_comms AFTER INSERT ON 
senders_comms FOR EACH ROW EXCUTE extract_senders_comms();


CREATE OR REPLACE FUNCTION extract_senders_comms () RETURNS void AS
'
DECLARE
    l_msgtype int;
    l_msgstatus int;
BEGIN

    IF NEW.destination = 1 THEN        
        
        SELECT SUBSTRING(new.mdata, 1, 4)::int, 
        SUBSTRING(new.mdata, 5, 4)::int 
        INTO l_msgtype, l_msgstatus;
        
        IF msgtype = 3 THEN
            
            IF l_msgstatus = 3 THEN

                UPDATE connections_sendign 
                SET scstatus = l_msgstatus,
                fd = SUBSTRING(17, 4)::int
                WHERE ipaddr = SUBSTRING(new.mdata, 9, 8)::bigint;
            
            ELIF l_msgstatus = 1 THEN 

                UPDATE connections_sending 
                SET scstatus = l_msgstatus,
                WHERE ipaddr = SUBSTRING(new.mdata, 9, 8)::bigint;

            ENDIF;           

        ELIF msgtype = 4 THEN
        
            UPDATE send_data 
            SET status = l_msgstatus 
            WHERE sdid = SUBSTRING(NEW.mdata, 9, 4)::int;
        
        ENDIF;

        DELETE FROM senders_comms 
        WHERE scid = new.scid;   
    
    ENDIF;
END;
'
LANGUAGE 'plpgsql';



CREATE TABLE connections_sending (osid SERIAL PRIMARY KEY, 
                                  fd int NOT NULL,
                                  ipaddr bigint NOT NULL, 
                                  scstatus int NOT NULL);


----

CREATE TABLE incoming_msg_status (insid serial PRIMARY KEY,
                                  status int NOT NULL, 
                                  original_msgid text, 
                                  sfd int,
                                  required_data_amount int,
                                  total_data int);


CREATE TABLE raw_data (fd int NOT NULL, 
                       data text NOT NULL, 
                       data_size int NOT NULL,
                       priority int NOT NULL);


--CREATE TRIGGER tr_build_msg AFTER INSERT ON 
--raw_data FOR EACH ROW EXCUTE build_msg();

CREATE OR REPLACE FUNCTION build_msg () RETURNS void AS
'
DECLARE
    lfd int;
    ldata text;
    ldata_size int;
    ldata_read int;
    lmdata_size int;
BEGIN
    
    SELECT fd, data, data_size FROM raw_data into lfd, ldata, ldata_size;
    ldata_read := 0;

    LOOP UNTIL ldata_size - ldata_read >= 40
      
        SELECT into lmdata_size SUBSTRING(ldata, 1, 4);
        IF ldata_size - ldata_read >= lmdata_size THEN
            
            INSERT INTO new_msg (nmdata_size, nmdata) 
            VALUES lmdata_size, (SUBSTRING(ldata, ldata_read+1, lmdata_size));

            ldata_read := ldata_read + lmdata_size;
    ENDLOOP;

    IF ldata_read > 0 THEN
        
        UPDATE raw_data 
        SET data = SUBSTRING(data, ldata_read+1, ldata_size - ldata_read),
        ldata_size = ldata_read, 
        priority = 10
        WHERE fd = lfd;

    ENDIF;

END;
'
LANGUAGE 'plpgsql';


CREATE TABLE new_msg (nmgid SERIAL PRIMARY KEY, 
                      nmdata_size int NOT NULL,
                      nmdata text NOT NULL);


CREATE TRIGGER tr_extract_msg_info AFTER INSERT ON 
new_msg FOR EACH ROW EXCUTE extract_msg_info();


CREATE OR REPLACE FUNCTION extract_msg_info () RETURNS void AS
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

'
LANGUAGE 'plpgsql';

CREATE TABLE query (queryid int PRIMARY KEY,
                    query text);

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

CREATE TABLE msg_chunk (msgid text PRIMARY KEY, 
                        source bigint NOT NULL, 
                        destination bigint NOT NULL,
                        mpriority int NOT NULL, 
                        mtype int NOT NULL, 
                        original_msgid text NOT NULL, 
                        chunk_number int NOT NULL, 
                        size int NOT NULL, 
                        msg_status int NOT NULL,
                        mdata text NOT NULL);

CREATE TABLE msg_info (miid text PRIMARY KEY, 
                       source bigint NOT NULL, 
                       destination bigint NOT NULL,
                       mpriority int NOT NULL, 
                       mtype int NOT NULL, 
                       origingl_msgid text NOT NULL, 
                       total_parts int NOT NULL,
                       total_size int NOT NULL, 
                       time_to_receive int NOT NULL);

CREATE TABLE get_info (msgid text PRIMARY KEY,
                        mtype int NOT NULL,
                        data_size int,
                        mdata text);

CREATE TABLE for_others (fomid serial PRIMARY KEY,
                         destination bigint NOT NULL,
                         mpriority int NOT NULL,
                         msg text NOT NULL,
                         data_size int NOT NULL, 
                         );

CREATE TABLE send_data (sdid SERIAL PRIMARY KEY, 
                        fd int NOT NULL, 
                        sdata text NOT NULL, 
                        data_size int NOT NULL, 
                        mstatus int NOT NULL);



--insert into data(d1, d2) select  substring(data, 1,10)::int as da1, substring(data, 11,20)::int as da2 from tm1;

--copy tm1(data) FROM '/home/shrikant/gb.txt';



-- raw_data will get data from receiver : path = receiver.c -> processor.c -> raw_data

-- data from send_data will be for sender : path = send_data -> processor.c -> sender.c
    -- status 1 = data need to be send
    -- status 2 = sending in progress
    -- status 3 = sending failed

-- connections_sending : 
    -- for connections sending 
        -- status 1 = need to be opened
        -- status 2 = trying to open
        -- status 3 = opened
        -- status 4 = need to be closed
        -- status 5 = closed

-- senders_comms
    -- 2 columns: data and destination (DEFAULT 1)
        -- if destination = 1 then message is intended for db
        -- if destination = 2 then message is meant for sender

    -- it can have 4 types of messages

        -- msgtype = 1 open a connection
            -- it will consist of (msgtype, ipaddr)
            -- from db -> sender
    
        -- msgtype = 2 close a connection
            -- it will consist of (msgtype, fd)
            --  from db -> sender
    
        -- msgtype = 3 connection opening status
            -- from sender -> db 
            -- if status = 3 means connection opened
            -- (msgytpe, status, ipaddr, fd)
            -- if status = 1 means conneciton opening failed
            -- (msgtype, status,  ipaddr)
    
        -- msgtyp = 4 msg sending status
            -- from sender -> db
            -- consist of (msgtype, msgstatus, msgid)
            -- if msgtype

-- connections_receiving :
    -- status 1 will indicate connection opened
    -- status 2 will indicate connection closed

-- receiver_comms :
    -- stores communications received from receiver with status = 1
    -- stores communications intended to be sent to 

-- if destination = 1 then message is intended for db,  1 will be default value    -- 2 columns: data and destination(DEFAULT 1)
        
        -- if destination = 2 then message is meant for receiver

    -- receiver will inform db about a new connection's opening
        -- INSERT INTO receiver_comms (data) VALUES ($1);
        -- if msg type = 1, then it is about opening of new connection
                -- it will contain msg type, fd and ipaddress 
        
    -- receiver will inform db about a new connection's closing
        -- INSERT INTO receiver_comms (data) VALUES ($1);
        -- if msg type = 2, then it is about closing of a connection
                -- it will contain msg type, fd 

    -- db will inform receiver to close a connection
        -- SELECT data FROM receiver_comms where destination = 2; 
        -- if msg type = 3, then db is informing receiver to close a connection
            -- it will contain msg type, fd 
 
    -- db will inquire about status of a connection
        -- SELECT data FROM receiver_comms where destination = 2;
        -- if msg type = 4, then db is inquiring receiver about status of a connection
            -- it will contain msg type, fd 

        -- in reply to this receiver will send a message about status of type 1


-- incoming status if status = 1 means read, 2 means unread
--raws data
-- check status 
    -- rstaus = 1
        -- read header
        -- store header and data

    -- rstatus = 2 also read msgid and remaining data
        -- read remaining data
        -- update data

    -- 1 if didnt found enough data
    --   1.1 set rstatus = 2 and store msgid
    --   1.2 set total data and remaining data
    --   1.3 also mark the message as incomplete
    -- 2 else mark data as complete and set rstatus = 1


-- nm table
    -- status = 1 means incomplete
    -- status = 2 means complete




-- CREATE TABLE status_r (msid text PRIMARY KEY, 
--                        source bigint NOT NULL, 
--                        destination bigint NOT NULL,
--                        mpriority int NOT NULL, 
--                        mtype int NOT NULL, 
--                        original_msgid text NOT NULL, 
--                        mstatus int NOT NULL, 
--                        size int NOT NULL, 
--                        mdata text);

-- CREATE TABLE get_system_info (msid text PRIMARY KEY, 
--                               source bigint NOT NULL, 
--                               destination bigint NOT NULL,
--                               mpriority int NOT NULL, 
--                               mtype int NOT NULL, 
--                               info required text NOT NULL, 
--                               message_type int NOT NULL);

-- CREATE TABLE message_status (msid text PRIMARY KEY, 
--                               source bigint NOT NULL, 
--                               destination bigint NOT NULL,
--                               mpriority int NOT NULL, 
--                               mtype int NOT NULL,
--                               original_msgid text NOT NULL);
