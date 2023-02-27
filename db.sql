drop table raw_data;
drop table send_data;
drop table  open_connections;
drop table  senders_comm;
drop table  for_receiver;
drop table  for_sender;


create table raw_data(rdid serial primary key, fd int NOT NULL, data bytea NOT NULL, data_size int NOT NULL);
create table send_data(sdid serial primary key, fd int NOT NULL, data bytea NOT NULL, data_size int NOT NULL, status int NOT NULL);
create table connections_receiving(orid serial primary key, fd int NOT NULL, ipaddr int NOT NULL, status int NOT NULL);
create table connections_sending(osid serial primary key, fd int NOT NULL, ipaddr int NOT NULL, status int NOT NULL);
create table receivers_comms(rcid serial primary key, data bytea NOT NULL, destination int NOT NULL);
create table senders_comms(scid serial primary key, data bytea NOT NULL, status int NOT NULL);


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









