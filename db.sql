drop table raw_data;
drop table raw_data;
drop table send_data;
drop table connections_receiving;
drop table connections_sending;
drop table receivers_comms;
drop table senders_comms;
drop table msg_chunk;
drop table msg_info;
drop table status_r;
drop table get_system_info;
drop table message_status;

CREATE TABLE receivers_comms (rcid SERIAL PRIMARY KEY, 
                              mdata bytea NOT NULL, 
                              destination bigint NOT NULL);

CREATE OR REPLACE FUNCTION extract_receivers_comms () RETURNS VOID AS
'
DECLARE
    l_mdata bytea;
    l_msgtype int;
    l_fd int;
    l_ipaddr bigint;
BEGIN
    SELECT mdata INTO l_mdata FROM receivers_comms WHERE destination = 1 LIMIT 1;
    
    if rcstatus = 1 then
        insert into connections_receiving
END;
'
LANGUAGE 'plpgsql';

insert into data(d1, d2) select  substring(data, 1,10)::int as da1, substring(data, 11,20)::int as da2 from tm1;

copy tm1(data) FROM '/home/shrikant/gb.txt';


CREATE TABLE senders_comms (scid SERIAL PRIMARY KEY, 
                            mdata text NOT NULL, 
                            mstatus int NOT NULL);

CREATE TABLE connections_receiving (orid SERIAL PRIMARY KEY, 
                                    fd int NOT NULL, 
                                    ipaddr bigint NOT NULL, 
                                    mstatus int NOT NULL);

CREATE TABLE connections_sending (osid SERIAL PRIMARY KEY, 
                                  fd int NOT NULL,
                                  ipaddr bigint NOT NULL, 
                                  mstatus int NOT NULL);

CREATE TABLE raw_data (rdid SERIAL PRIMARY KEY, 
                       fd int NOT NULL, data bytea NOT NULL, 
                       data_size int NOT NULL);

CREATE TABLE msg_chunk (msgid text PRIMARY KEY, 
                        source bigint NOT NULL, 
                        destination bigint NOT NULL,
                        mpriority int NOT NULL, 
                        mtype int NOT NULL, 
                        original_msgid text NOT NULL, 
                        chunk_number int NOT NULL, 
                        size int NOT NULL, 
                        mdata bytea NOT NULL,
                        msg_status int NOT NULL);

CREATE TABLE msg_info (miid text PRIMARY KEY, 
                       source bigint NOT NULL, 
                       mpriority int NOT NULL, 
                       mtype int NOT NULL, 
                       origingl_msgid text NOT NULL, 
                       total_parts int NOT NULL,
                       total_size int NOT NULL, 
                       time_to_receive int NOT NULL);

CREATE TABLE status_r (msid text PRIMARY KEY, 
                       source bigint NOT NULL, 
                       destination bigint NOT NULL,
                       mpriority int NOT NULL, 
                       mtype int NOT NULL, 
                       original_msgid text NOT NULL, 
                       mstatus int NOT NULL, 
                       size int NOT NULL, mdata bytea);

CREATE TABLE get_system_info (msid text PRIMARY KEY, 
                              source bigint NOT NULL, 
                              destination bigint NOT NULL,
                              mpriority int NOT NULL, 
                              mtype int NOT NULL, 
                              info required bytea NOT NULL, 
                              message_type int NOT NULL);

CREATE TABLE message_status (msid text PRIMARY KEY, 
                              source bigint NOT NULL, 
                              destination bigint NOT NULL,
                              mpriority int NOT NULL, 
                              mtype int NOT NULL,
                              original_msgid text NOT NULL);

CREATE TABLE send_data (sdid SERIAL PRIMARY KEY, 
                        fd int NOT NULL, data bytea NOT NULL, 
                        data_size int NOT NULL, 
                        mstatus int NOT NULL);























-- raw_data will get data from receiver : path = receiver.c -> processor.c -> raw_data

-- data from send_data will be for sender : path = send_data -> processor.c -> sender.c
    -- status 1 = data need to be send
    -- status 2 = sending in progress
    -- status 3 = sending failed

-- connections_receiving :
    -- status 1 will indicate connection opened
    -- status 2 will indicate connection closed

-- connections_sending : 
    -- for connections sending 
        -- status 1 = need to be opened
        -- status 2 = opened
        -- status 3 = need to be closed
        -- status 4 = closed

-- receiver_comms :
    -- stores communications received from receiver with status = 1
    -- stores communications intended to be sent to 

    -- 2 columns: data and destination
        -- if destination = 1 then message is intended for db,  1 will be default value
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






